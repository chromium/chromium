// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_POST_TASK_H_
#define BASE_BIND_POST_TASK_H_

#include <memory>
#include <type_traits>

#include "base/bind.h"
#include "base/bind_post_task_internal.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"

// BindPostTask() is a helper function for binding a OnceCallback or
// RepeatingCallback to a task runner. BindPostTask(task_runner, callback)
// returns a task runner bound callback with an identical type to |callback|.
// The returned callback will take the same arguments as the input |callback|.
// Invoking Run() on the returned callback will post a task to run |callback| on
// target |task_runner| with the provided arguments.
//
// This is typically used when a callback must be invoked on a specific task
// runner but is provided as a result callback to a function that runs
// asynchronously on a different task runner.
//
// Example:
//    // |result_cb| can only be safely run on |my_task_runner|.
//    auto result_cb = BindOnce(&Foo::ReceiveReply, foo);
//    // Note that even if |returned_cb| is never run |result_cb| will attempt
//    // to be destroyed on |my_task_runner|.
//    auto returned_cb = BindPostTask(my_task_runner, std::move(result_cb));
//    // RunAsyncTask() will run the provided callback upon completion.
//    other_task_runner->PostTask(FROM_HERE,
//                                BindOnce(&RunAsyncTask,
//                                         std::move(returned_cb)));
//
// If the example provided |result_cb| to RunAsyncTask() then |result_cb| would
// run unsafely on |other_task_runner|. Instead RunAsyncTask() will run
// |returned_cb| which will post a task to |current_task_runner| before running
// |result_cb| safely.
//
// An alternative approach in the example above is to change RunAsyncTask() to
// also take a task runner as an argument and have RunAsyncTask() post the task.
// For cases where that isn't desirable BindPostTask() provides a convenient
// alternative.
//
// The input |callback| will always attempt to be destroyed on the target task
// runner. Even if the returned callback is never invoked, a task will be posted
// to destroy the input |callback|. However, if the target task runner has
// shutdown this is no longer possible. PostTask() will return false and the
// callback will be destroyed immediately on the current thread.
//
// The input |callback| must have a void return type to be compatible with
// PostTask(). If you want to drop the callback return value then use
// base::IgnoreResult(&Func) when creating the input |callback|.

namespace base {

// Creates a OnceCallback that will run |callback| on |task_runner|. If the
// returned callback is destroyed without being run then |callback| will be
// be destroyed on |task_runner|.
template <typename ReturnType, typename... Args>
OnceCallback<void(Args...)> BindPostTask(
    scoped_refptr<TaskRunner> task_runner,
    OnceCallback<ReturnType(Args...)> callback,
    const Location& location = FROM_HERE) {
  static_assert(std::is_same<ReturnType, void>::value,
                "OnceCallback must have void return type in order to produce a "
                "closure for PostTask(). Use base::IgnoreResult() to drop the "
                "return value if desired.");

  using Helper = internal::BindPostTaskTrampoline<OnceCallback<void(Args...)>>;

  return base::BindOnce(
      &Helper::template Run<Args...>,
      std::make_unique<Helper>(std::move(task_runner), location,
                               std::move(callback)));
}

// Creates a RepeatingCallback that will run |callback| on |task_runner|. When
// the returned callback is destroyed a task will be posted to destroy the input
// |callback| on |task_runner|.
template <typename ReturnType, typename... Args>
RepeatingCallback<void(Args...)> BindPostTask(
    scoped_refptr<TaskRunner> task_runner,
    RepeatingCallback<ReturnType(Args...)> callback,
    const Location& location = FROM_HERE) {
  static_assert(std::is_same<ReturnType, void>::value,
                "RepeatingCallback must have void return type in order to "
                "produce a closure for PostTask(). Use base::IgnoreResult() to "
                "drop the return value if desired.");

  using Helper =
      internal::BindPostTaskTrampoline<RepeatingCallback<void(Args...)>>;

  return base::BindRepeating(
      &Helper::template Run<Args...>,
      std::make_unique<Helper>(std::move(task_runner), location,
                               std::move(callback)));
}

}  // namespace base

#endif  // BASE_BIND_POST_TASK_H_
