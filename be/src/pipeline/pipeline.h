// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <atomic>
#include <vector>

#include "common/status.h"
#include "exec/operator.h"
#include "vec/core/block.h"

namespace doris::pipeline {

class Pipeline;
using PipelinePtr = std::shared_ptr<Pipeline>;
using Pipelines = std::vector<PipelinePtr>;
using PipelineId = uint32_t;

class PipelineTask;
class PipelineFragmentContext;

class Pipeline : public std::enable_shared_from_this<Pipeline> {
    friend class PipelineTask;

public:
    Pipeline() = delete;
    explicit Pipeline(PipelineId pipeline_id, std::shared_ptr<PipelineFragmentContext> context)
            : _complete_dependency(0), _pipeline_id(pipeline_id), _context(std::move(context)) {}

    // 检查从root到source是一串
    Status prepare(RuntimeState* state);

    void close(RuntimeState*);

    void add_dependency(std::shared_ptr<Pipeline>& pipeline) {
        pipeline->_parents.push_back(shared_from_this());
        _dependencies.push_back(pipeline);
    }

    // 如果所有依赖都结束，其状态是BLOCKED，则将Task提交到TaskScheduler
    // pipeline执行完成要调用父pipeline的该方法
    // 例如hash build完成后要调用hash probe的finish_one_dependency方法
    bool finish_one_dependency() {
        DCHECK(_complete_dependency < _dependencies.size());
        return _complete_dependency.fetch_add(1) == _dependencies.size() - 1;
    }

    bool has_dependency() { return _complete_dependency.load() < _dependencies.size(); }

    // 把source插到operator中
    Status set_source(OperatorTemplatePtr& source_operator);

    Status add_operator(OperatorTemplatePtr& op);

    Status set_sink(OperatorTemplatePtr& sink_operator);

    OperatorTemplate* sink() { return _sink.get(); }

    Status build_operators(Operators&);

    RuntimeProfile* runtime_profile() { return _pipeline_profile.get(); }

private:
    std::atomic<uint32_t> _complete_dependency;

    OperatorTemplatePtr _source;
    OperatorTemplates _operators; // left is _source, right is _root
    OperatorTemplatePtr _sink;    // put block to sink

    std::vector<std::shared_ptr<Pipeline>> _parents;
    std::vector<std::shared_ptr<Pipeline>> _dependencies;

    PipelineId _pipeline_id;
    std::shared_ptr<PipelineFragmentContext> _context;

    std::unique_ptr<RuntimeProfile> _pipeline_profile;
};

} // namespace doris::pipeline