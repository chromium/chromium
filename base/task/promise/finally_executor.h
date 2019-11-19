// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_FINALLY_EXECUTOR_H_
#define BASE_TASK_PROMISE_FINALLY_EXECUTOR_H_

#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"

namespace base {
namespace internal {

// Exists to reduce template bloat.
class BASE_EXPORT FinallyExecutorCommon {
 public:
  explicit FinallyExecutorCommon(CallbackBase&& callback) noexcept;
  ~FinallyExecutorCommon();

  // PromiseExecutor:
  bool IsCancelled() const;

  CallbackBase callback_;
};

// A finally promise executor runs regardless of whether the prerequisite was
// resolved or rejected. If the prerequsite is cancelled, the finally promise
// and any dependents are cancelled too.
template <typename CallbackT, typename ResolveStorage, typename RejectStorage>
class FinallyExecutor {
 public:
  using CallbackReturnT = typename CallbackTraits<CallbackT>::ReturnType;

  explicit FinallyExecutor(CallbackBase&& callback) noexcept
      : common_(std::move(callback)) {}

  ~FinallyExecutor() = default;

  bool IsCancelled() const { return common_.IsCancelled(); }

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PromiseExecutor::PrerequisitePolicy::kAll;

  void Execute(AbstractPromise* promise) {
    AbstractPromise* prerequisite = promise->GetOnlyPrerequisite();
    // Internally RunHelper uses const RepeatingCallback<>& to avoid the
    // binary size overhead of moving a scoped_refptr<> about.  We respect
    // the onceness of the callback and RunHelper will overwrite the callback
    // with the result.
    using RepeatingCB = typename ToRepeatingCallback<CallbackT>::value;
    RepeatingCB* resolve_executor =
        static_cast<RepeatingCB*>(&common_.callback_);
    RunHelper<RepeatingCB, void, ResolveStorage, RejectStorage>::Run(
        *resolve_executor, prerequisite, promise);
  }

#if DCHECK_IS_ON()
  PromiseExecutor::ArgumentPassingType ResolveArgumentPassingType() const {
    return PromiseExecutor::ArgumentPassingType::kNormal;
  }

  PromiseExecutor::ArgumentPassingType RejectArgumentPassingType() const {
    return PromiseExecutor::ArgumentPassingType::kNormal;
  }

  bool CanResolve() const {
    return PromiseCallbackTraits<CallbackReturnT>::could_resolve;
  }

  bool CanReject() const {
    return PromiseCallbackTraits<CallbackReturnT>::could_reject;
  }
#endif

 private:
  FinallyExecutorCommon common_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_FINALLY_EXECUTOR_H_
