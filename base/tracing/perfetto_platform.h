// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACING_PERFETTO_PLATFORM_H_
#define BASE_TRACING_PERFETTO_PLATFORM_H_

#include "third_party/perfetto/include/perfetto/tracing/platform.h"

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local_storage.h"

namespace base {
class DeferredSequencedTaskRunner;

namespace tracing {

class BASE_EXPORT PerfettoPlatform : public perfetto::Platform,
                                     ThreadIdNameManager::Observer {
 public:
  // Specifies the type of task runner used by Perfetto.
  // TODO(skyostil): Move all scenarios to use the default task runner to
  // avoid problems with unexpected re-entrancy and IPC deadlocks.
  enum class TaskRunnerType {
    // Use Perfetto's own task runner which runs tasks on a dedicated (internal)
    // thread.
    kBuiltin,
    // Use base::ThreadPool.
    kThreadPool,
  };

  explicit PerfettoPlatform(TaskRunnerType = TaskRunnerType::kThreadPool);
  ~PerfettoPlatform() override;

  SequencedTaskRunner* task_runner() const;
  bool did_start_task_runner() const { return did_start_task_runner_; }
  void StartTaskRunner(scoped_refptr<SequencedTaskRunner>);

  // perfetto::Platform implementation:
  ThreadLocalObject* GetOrCreateThreadLocalObject() override;
  std::unique_ptr<perfetto::base::TaskRunner> CreateTaskRunner(
      const CreateTaskRunnerArgs&) override;
  std::string GetCurrentProcessName() override;

  // ThreadIdNameManager::Observer implementation.
  void OnThreadNameChanged(const char* name) override;

 private:
  const TaskRunnerType task_runner_type_;
  scoped_refptr<DeferredSequencedTaskRunner> deferred_task_runner_;
  bool did_start_task_runner_ = false;
  ThreadLocalStorage::Slot thread_local_object_;
};

}  // namespace tracing
}  // namespace base

#endif  // BASE_TRACING_PERFETTO_PLATFORM_H_
