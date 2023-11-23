// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_INTERFACE_H_
#define BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_INTERFACE_H_

#include "base/task/post_job.h"
#include "base/task/thread_pool/task_source.h"

namespace base {

class TaskAnnotator;

namespace internal {

class PooledTaskRunnerDelegate;

// Interface for a job task source, to facilitate using either JobTaskSourceNew
// or JobTaskSourceOld depending on the UseNewJobImplementation feature state.
class BASE_EXPORT JobTaskSource : public TaskSource {
 public:
  JobTaskSource(const JobTaskSource&) = delete;
  JobTaskSource& operator=(const JobTaskSource&) = delete;

  static JobHandle CreateJobHandle(
      scoped_refptr<internal::JobTaskSource> task_source) {
    return JobHandle(std::move(task_source));
  }

  virtual void WillEnqueue(int sequence_num, TaskAnnotator& annotator) = 0;
  virtual bool NotifyConcurrencyIncrease() = 0;
  virtual bool WillJoin() = 0;
  virtual bool RunJoinTask() = 0;
  virtual void Cancel(TaskSource::Transaction* transaction = nullptr) = 0;
  virtual bool IsActive() const = 0;
  virtual size_t GetWorkerCount() const = 0;
  virtual size_t GetMaxConcurrency() const = 0;
  virtual uint8_t AcquireTaskId() = 0;
  virtual void ReleaseTaskId(uint8_t task_id) = 0;
  virtual bool ShouldYield() = 0;
  virtual PooledTaskRunnerDelegate* GetDelegate() const = 0;

 protected:
  JobTaskSource(const TaskTraits& traits,
                TaskRunner* task_runner,
                TaskSourceExecutionMode execution_mode)
      : TaskSource(traits, task_runner, execution_mode) {}
  ~JobTaskSource() override = default;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_INTERFACE_H_
