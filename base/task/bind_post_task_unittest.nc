// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/bind_post_task.h"

#include "base/task/sequenced_task_runner.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace base {

int ReturnInt() {
  return 5;
}

#if defined(NCTEST_ONCE_NON_VOID_RETURN_BIND_POST_TASK)  // [r"fatal error: static assertion failed due to requirement 'std::is_same<int, void>::value': OnceCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired."]
// OnceCallback with non-void return type.
void WontCompile() {
  OnceCallback<int()> cb = BindOnce(&ReturnInt);
  auto post_cb = BindPostTask(SequencedTaskRunner::GetCurrentDefault(), std::move(cb));
  std::move(post_cb).Run();
}

#elif defined(NCTEST_REPEATING_NON_VOID_RETURN_BIND_POST_TASK)  // [r"fatal error: static assertion failed due to requirement 'std::is_same<int, void>::value': RepeatingCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired."]
// RepeatingCallback with non-void return type.
void WontCompile() {
  RepeatingCallback<int()> cb = BindRepeating(&ReturnInt);
  auto post_cb = BindPostTask(SequencedTaskRunner::GetCurrentDefault(), std::move(cb));
  std::move(post_cb).Run();
}

#elif defined(NCTEST_ONCE_NON_VOID_RETURN_BIND_POST_TASK_TO_CURRENT_DEFAULT)  // [r"fatal error: static assertion failed due to requirement 'std::is_same<int, void>::value': OnceCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired."]
// OnceCallback with non-void return type.
void WontCompile() {
  OnceCallback<int()> cb = BindOnce(&ReturnInt);
  auto post_cb = BindPostTaskToCurrentDefault(std::move(cb));
  std::move(post_cb).Run();
}

#elif defined(NCTEST_REPEATING_NON_VOID_RETURN_BIND_POST_TASK_TO_CURRENT_DEFAULT)  // [r"fatal error: static assertion failed due to requirement 'std::is_same<int, void>::value': RepeatingCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired."]
// RepeatingCallback with non-void return type.
void WontCompile() {
  RepeatingCallback<int()> cb = BindRepeating(&ReturnInt);
  auto post_cb = BindPostTaskToCurrentDefault(std::move(cb));
  std::move(post_cb).Run();
}

#endif

}  // namespace base
