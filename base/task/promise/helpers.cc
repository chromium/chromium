// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/helpers.h"

#include "base/bind_helpers.h"
#include "base/task/promise/no_op_promise_executor.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {
namespace internal {

DoNothing ToCallbackBase(DoNothing task) {
  return task;
}

PassedPromise ConstructAbstractPromiseWithSinglePrerequisite(
    const scoped_refptr<TaskRunner>& task_runner,
    const Location& from_here,
    AbstractPromise* prerequisite,
    internal::PromiseExecutor::Data&& executor_data) noexcept {
  // Note |prerequisite| can legitimately be null when posting a promise chain
  // during shutdown.
  if (!prerequisite) {
    // Ensure the destructor for |executor_data| runs.
    PromiseExecutor dummy_executor(std::move(executor_data));
    return PassedPromise();
  }

  return PassedPromise(AbstractPromise::Create(
      task_runner, from_here,
      std::make_unique<AbstractPromise::AdjacencyList>(prerequisite),
      RejectPolicy::kMustCatchRejection,
      internal::DependentList::ConstructUnresolved(),
      std::move(executor_data)));
}

PassedPromise ConstructHereAbstractPromiseWithSinglePrerequisite(
    const Location& from_here,
    AbstractPromise* prerequisite,
    internal::PromiseExecutor::Data&& executor_data) noexcept {
  return ConstructAbstractPromiseWithSinglePrerequisite(
      SequencedTaskRunnerHandle::Get(), from_here, prerequisite,
      std::move(executor_data));
}

PassedPromise ConstructManualPromiseResolverPromise(const Location& from_here,
                                                    RejectPolicy reject_policy,
                                                    bool can_resolve,
                                                    bool can_reject) {
  return PassedPromise(AbstractPromise::CreateNoPrerequisitePromise(
      from_here, reject_policy, internal::DependentList::ConstructUnresolved(),
      internal::PromiseExecutor::Data(
          in_place_type_t<internal::NoOpPromiseExecutor>(), can_resolve,
          can_reject)));
}

}  // namespace internal
}  // namespace base
