// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
#define BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_

#include <type_traits>

#include "base/callback.h"
#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"

namespace base {
namespace internal {

// Exists to reduce template bloat.
class BASE_EXPORT ThenAndCatchExecutorCommon {
 public:
  ThenAndCatchExecutorCommon(internal::CallbackBase&& then_callback,
                             internal::CallbackBase&& catch_callback) noexcept
      : then_callback_(std::move(then_callback)),
        catch_callback_(std::move(catch_callback)) {
    DCHECK(!then_callback_.is_null() || !catch_callback_.is_null());
  }

  ~ThenAndCatchExecutorCommon() = default;

  // PromiseExecutor:
  bool IsCancelled() const;
  PromiseExecutor::PrerequisitePolicy GetPrerequisitePolicy() const;

  using ExecuteCallback = void (*)(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* callback);

  void Execute(AbstractPromise* promise,
               ExecuteCallback execute_then,
               ExecuteCallback execute_catch);

  // If |executor| is null then the value of |arg| is moved or copied into
  // |result| and true is returned. Otherwise false is returned.
  static bool ProcessNullCallback(const CallbackBase& executor,
                                  AbstractPromise* arg,
                                  AbstractPromise* result);

  CallbackBase then_callback_;
  CallbackBase catch_callback_;
};

// Tag signals no callback which is used to eliminate dead code.
struct NoCallback {};

template <typename ThenOnceCallback,
          typename CatchOnceCallback,
          typename ArgResolve,
          typename ArgReject,
          typename ResolveStorage,
          typename RejectStorage>
class ThenAndCatchExecutor {
 public:
  using ThenReturnT = typename CallbackTraits<ThenOnceCallback>::ReturnType;
  using CatchReturnT = typename CallbackTraits<CatchOnceCallback>::ReturnType;
  using PrerequisiteCouldResolve =
      std::integral_constant<bool,
                             !std::is_same<ArgResolve, NoCallback>::value>;
  using PrerequisiteCouldReject =
      std::integral_constant<bool, !std::is_same<ArgReject, NoCallback>::value>;

  ThenAndCatchExecutor(CallbackBase&& resolve_callback,
                       CallbackBase&& catch_callback) noexcept
      : common_(std::move(resolve_callback), std::move(catch_callback)) {}

  bool IsCancelled() const { return common_.IsCancelled(); }

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PromiseExecutor::PrerequisitePolicy::kAll;

  using ExecuteCallback = ThenAndCatchExecutorCommon::ExecuteCallback;

  void Execute(AbstractPromise* promise) {
    return common_.Execute(promise, &ExecuteThen, &ExecuteCatch);
  }

#if DCHECK_IS_ON()
  PromiseExecutor::ArgumentPassingType ResolveArgumentPassingType() const {
    return common_.then_callback_.is_null()
               ? PromiseExecutor::ArgumentPassingType::kNoCallback
               : CallbackTraits<ThenOnceCallback>::argument_passing_type;
  }

  PromiseExecutor::ArgumentPassingType RejectArgumentPassingType() const {
    return common_.catch_callback_.is_null()
               ? PromiseExecutor::ArgumentPassingType::kNoCallback
               : CallbackTraits<CatchOnceCallback>::argument_passing_type;
  }

  bool CanResolve() const {
    return (!common_.then_callback_.is_null() &&
            PromiseCallbackTraits<ThenReturnT>::could_resolve) ||
           (!common_.catch_callback_.is_null() &&
            PromiseCallbackTraits<CatchReturnT>::could_resolve);
  }

  bool CanReject() const {
    return (!common_.then_callback_.is_null() &&
            PromiseCallbackTraits<ThenReturnT>::could_reject) ||
           (!common_.catch_callback_.is_null() &&
            PromiseCallbackTraits<CatchReturnT>::could_reject);
  }
#endif

 private:
  static void ExecuteThen(AbstractPromise* prerequisite,
                          AbstractPromise* promise,
                          CallbackBase* resolve_callback) {
    ExecuteThenInternal(prerequisite, promise, resolve_callback,
                        PrerequisiteCouldResolve());
  }

  static void ExecuteCatch(AbstractPromise* prerequisite,
                           AbstractPromise* promise,
                           CallbackBase* catch_callback) {
    ExecuteCatchInternal(prerequisite, promise, catch_callback,
                         PrerequisiteCouldReject());
  }

  static void ExecuteThenInternal(AbstractPromise* prerequisite,
                                  AbstractPromise* promise,
                                  CallbackBase* resolve_callback,
                                  std::true_type can_resolve) {
    // Internally RunHelper uses const RepeatingCallback<>& to avoid the
    // binary size overhead of moving a scoped_refptr<> about.  We respect
    // the onceness of the callback and RunHelper will overwrite the callback
    // with the result.
    using RepeatingThenCB =
        typename ToRepeatingCallback<ThenOnceCallback>::value;
    RunHelper<
        RepeatingThenCB, Resolved<ArgResolve>, ResolveStorage,
        RejectStorage>::Run(*static_cast<RepeatingThenCB*>(resolve_callback),
                            prerequisite, promise);
  }

  static void ExecuteThenInternal(AbstractPromise* prerequisite,
                                  AbstractPromise* promise,
                                  CallbackBase* resolve_callback,
                                  std::false_type can_resolve) {
    // |prerequisite| can't resolve so don't generate dead code.
  }

  static void ExecuteCatchInternal(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* catch_callback,
                                   std::true_type can_reject) {
    // Internally RunHelper uses const RepeatingCallback<>& to avoid the
    // binary size overhead of moving a scoped_refptr<> about.  We respect
    // the onceness of the callback and RunHelper will overwrite the callback
    // with the result.
    using RepeatingCatchCB =
        typename ToRepeatingCallback<CatchOnceCallback>::value;
    RunHelper<
        RepeatingCatchCB, Rejected<ArgReject>, ResolveStorage,
        RejectStorage>::Run(*static_cast<RepeatingCatchCB*>(catch_callback),
                            prerequisite, promise);
  }

  static void ExecuteCatchInternal(AbstractPromise* prerequisite,
                                   AbstractPromise* promise,
                                   CallbackBase* catch_callback,
                                   std::false_type can_reject) {
    // |prerequisite| can't reject so don't generate dead code.
  }

  ThenAndCatchExecutorCommon common_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
