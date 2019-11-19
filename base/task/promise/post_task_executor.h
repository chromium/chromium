// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_POST_TASK_EXECUTOR_H_
#define BASE_TASK_PROMISE_POST_TASK_EXECUTOR_H_

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"

namespace base {
namespace internal {

// PromiseExecutor for use by PostTask.
template <typename ReturnType>
class PostTaskExecutor {
 public:
  // Extract properties from |ReturnType|.
  using CallbackTraits = PromiseCallbackTraits<ReturnType>;
  using ReturnedPromiseResolveT = typename CallbackTraits::ResolveType;
  using ReturnedPromiseRejectT = typename CallbackTraits::RejectType;
  using ResolveStorage = Resolved<ReturnedPromiseResolveT>;
  using RejectStorage = Rejected<ReturnedPromiseRejectT>;

  explicit PostTaskExecutor(CallbackBase&& task) noexcept
      : task_(std::move(task)) {}

  explicit PostTaskExecutor(DoNothing task) noexcept : task_(task.Once()) {}

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PromiseExecutor::PrerequisitePolicy::kAll;

  bool IsCancelled() const { return task_.IsCancelled(); }

#if DCHECK_IS_ON()
  PromiseExecutor::ArgumentPassingType ResolveArgumentPassingType() const {
    return PromiseExecutor::ArgumentPassingType::kNoCallback;
  }

  PromiseExecutor::ArgumentPassingType RejectArgumentPassingType() const {
    return PromiseExecutor::ArgumentPassingType::kNoCallback;
  }

  bool CanResolve() const { return CallbackTraits::could_resolve; }

  bool CanReject() const { return CallbackTraits::could_reject; }
#endif

  void Execute(AbstractPromise* promise) {
    static_assert(sizeof(CallbackBase) == sizeof(OnceCallback<ReturnType()>),
                  "We assume it's possible to cast from CallbackBase to "
                  "OnceCallback<ReturnType()>");
    // Internally RunHelper uses const RepeatingCallback<>& to avoid the
    // binary size overhead of moving a scoped_refptr<> about.  We respect
    // the onceness of the callback and RunHelper will overwrite the callback
    // with the result.
    RepeatingCallback<ReturnType()>* task =
        static_cast<RepeatingCallback<ReturnType()>*>(&task_);
    internal::RunHelper<RepeatingCallback<ReturnType()>, void, ResolveStorage,
                        RejectStorage>::Run(*task, nullptr, promise);
  }

 private:
  CallbackBase task_;

  DISALLOW_COPY_AND_ASSIGN(PostTaskExecutor);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_POST_TASK_EXECUTOR_H_
