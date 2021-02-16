// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/bind_post_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {

int ReturnInt() {
  return 5;
}

#if defined(NCTEST_ONCE_NON_VOID_RETURN)  // [r"fatal error: static_assert failed due to requirement 'std::is_same<int, void>::value' \"OnceCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired.\""]
// OnceCallback with non-void return type.
void WontCompile() {
  OnceCallback<int()> cb = BindOnce(&ReturnInt);
  auto post_cb = BindPostTask(SequencedTaskRunnerHandle::Get(), std::move(cb));
  std::move(post_cb).Run();
}

#elif defined(NCTEST_REPEATING_NON_VOID_RETURN)  // [r"fatal error: static_assert failed due to requirement 'std::is_same<int, void>::value' \"RepeatingCallback must have void return type in order to produce a closure for PostTask\(\). Use base::IgnoreResult\(\) to drop the return value if desired.\""]
// RepeatingCallback with non-void return type.
void WontCompile() {
  RepeatingCallback<int()> cb = BindRepeating(&ReturnInt);
  auto post_cb = BindPostTask(SequencedTaskRunnerHandle::Get(), std::move(cb));
  std::move(post_cb).Run();
}

#endif

}  // namespace base
