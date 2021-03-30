// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_POST_TASK_INTERNAL_H_
#define BASE_BIND_POST_TASK_INTERNAL_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"

namespace base {
namespace internal {

// Helper class to ensure that the input callback is always invoked and
// destroyed on the provided task runner.
template <typename CallbackType>
class BindPostTaskTrampoline {
 public:
  BindPostTaskTrampoline(scoped_refptr<TaskRunner> task_runner,
                         const Location& location,
                         CallbackType callback)
      : task_runner_(std::move(task_runner)),
        location_(location),
        callback_(std::move(callback)) {
    DCHECK(task_runner_);
    // Crash immediately instead of when trying to Run() `callback_` on the
    // destination `task_runner_`.
    CHECK(callback_);
  }

  BindPostTaskTrampoline(const BindPostTaskTrampoline& other) = delete;
  BindPostTaskTrampoline& operator=(const BindPostTaskTrampoline& other) =
      delete;

  ~BindPostTaskTrampoline() {
    if (callback_) {
      // Post a task to ensure that `callback_` is destroyed on `task_runner_`.
      // The callback's BindState may own an object that isn't threadsafe and is
      // unsafe to destroy on a different task runner.
      //
      // Note that while this guarantees `callback_` will be destroyed when the
      // posted task runs, it doesn't guarantee the ref-counted BindState is
      // destroyed at the same time. If the callback was copied before being
      // passed to BindPostTaskTrampoline then the BindState can outlive
      // `callback_`, so the user must ensure any other copies of the callback
      // are also destroyed on the correct task runner.
      task_runner_->PostTask(location_, BindOnce(&DestroyCallbackOnTaskRunner,
                                                 std::move(callback_)));
    }
  }

  template <typename... Args>
  void Run(Args... args) {
    // If CallbackType is a OnceCallback then GetClosure() consumes `callback_`.
    task_runner_->PostTask(location_,
                           GetClosure(&callback_, std::forward<Args>(args)...));
  }

 private:
  static OnceClosure GetClosure(OnceClosure* callback) {
    // `callback` is already a closure, no need to call BindOnce().
    return std::move(*callback);
  }

  template <typename... Args>
  static OnceClosure GetClosure(OnceCallback<void(Args...)>* callback,
                                Args&&... args) {
    return BindOnce(std::move(*callback), std::forward<Args>(args)...);
  }

  static OnceClosure GetClosure(RepeatingClosure* callback) {
    // `callback` is already a closure, no need to call BindOnce().
    return *callback;
  }

  template <typename... Args>
  static OnceClosure GetClosure(RepeatingCallback<void(Args...)>* callback,
                                Args&&... args) {
    return BindOnce(*callback, std::forward<Args>(args)...);
  }

  static void DestroyCallbackOnTaskRunner(CallbackType callback) {}

  const scoped_refptr<TaskRunner> task_runner_;
  const Location location_;
  CallbackType callback_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_BIND_POST_TASK_INTERNAL_H_
