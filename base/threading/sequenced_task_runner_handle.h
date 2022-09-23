// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SEQUENCED_TASK_RUNNER_HANDLE_H_
#define BASE_THREADING_SEQUENCED_TASK_RUNNER_HANDLE_H_

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

class ThreadTaskRunnerHandle;

class BASE_EXPORT SequencedTaskRunnerHandle {
 public:
  // DEPRECATED: Use SequencedTaskRunner::GetCurrentDefault() instead.
  // Returns a SequencedTaskRunner which guarantees that posted tasks will only
  // run after the current task is finished and will satisfy a SequenceChecker.
  // It should only be called if IsSet() returns true (see the comment there for
  // the requirements).
  [[nodiscard]] static const scoped_refptr<SequencedTaskRunner>& Get();

  // DEPRECATED: Use SequencedTaskRunner::HasCurrentDefault() instead.
  // Returns true if one of the following conditions is fulfilled:
  // a) A SequencedTaskRunner has been assigned to the current thread by
  //    instantiating a SequencedTaskRunnerHandle.
  // b) The current thread has a ThreadTaskRunnerHandle (which includes any
  //    thread that has a MessageLoop associated with it).
  [[nodiscard]] static bool IsSet();

  explicit SequencedTaskRunnerHandle(
      scoped_refptr<SequencedTaskRunner> task_runner)
      : contained_current_default_(std::move(task_runner)) {}

  SequencedTaskRunnerHandle(const SequencedTaskRunnerHandle&) = delete;
  SequencedTaskRunnerHandle& operator=(const SequencedTaskRunnerHandle&) =
      delete;

  ~SequencedTaskRunnerHandle() = default;

 private:
  SequencedTaskRunner::CurrentDefaultHandle contained_current_default_;
};

}  // namespace base

#endif  // BASE_THREADING_SEQUENCED_TASK_RUNNER_HANDLE_H_
