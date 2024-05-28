// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/test/trace_test_utils.h"

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/tracing/perfetto_platform.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"

namespace base {
namespace test {
namespace {

// A proxy task runner which can be dynamically pointed to route tasks into a
// different task runner.
class RebindableTaskRunner : public base::SequencedTaskRunner {
 public:
  RebindableTaskRunner();

  void set_task_runner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    task_runner_ = task_runner;
  }

  // base::SequencedTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

 private:
  ~RebindableTaskRunner() override;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

RebindableTaskRunner::RebindableTaskRunner() = default;
RebindableTaskRunner::~RebindableTaskRunner() = default;

bool RebindableTaskRunner::PostDelayedTask(const base::Location& from_here,
                                           base::OnceClosure task,
                                           base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, std::move(task), delay);
}

bool RebindableTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                  delay);
}

bool RebindableTaskRunner::RunsTasksInCurrentSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

RebindableTaskRunner* GetClientLibTaskRunner() {
  static base::NoDestructor<scoped_refptr<RebindableTaskRunner>> task_runner(
      MakeRefCounted<RebindableTaskRunner>());
  return task_runner.get()->get();
}

}  // namespace

TracingEnvironment::TracingEnvironment() {
  trace_event::TraceLog::GetInstance()->ResetForTesting();
}

TracingEnvironment::TracingEnvironment(
    TaskEnvironment& task_environment,
    scoped_refptr<SequencedTaskRunner> task_runner)
    : task_environment_(&task_environment) {
  // Since Perfetto's platform backend can only be initialized once in a
  // process, we give it a task runner that can outlive the per-test task
  // environment.
  auto* client_lib_task_runner = GetClientLibTaskRunner();
  client_lib_task_runner->set_task_runner(std::move(task_runner));

  // Wait for any posted construction tasks to execute.
  task_environment_->RunUntilIdle();
}

TracingEnvironment::~TracingEnvironment() {
  if (task_environment_) {
    // Wait for any posted destruction tasks to execute.
    task_environment_->RunUntilIdle();
  }
  perfetto::Tracing::ResetForTesting();
}

// static
perfetto::protos::gen::TraceConfig TracingEnvironment::GetDefaultTraceConfig() {
  perfetto::protos::gen::TraceConfig trace_config;
  auto* buffer_config = trace_config.add_buffers();
  buffer_config->set_size_kb(32 * 1024);
  auto* data_source = trace_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("track_event");
  source_config->set_target_buffer(0);
  return trace_config;
}

}  // namespace test
}  // namespace base
