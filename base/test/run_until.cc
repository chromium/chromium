// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"

#include <functional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_future.h"
#include "base/time/time_override.h"

namespace base::test {

namespace {

void TestPredicateOrRegisterOnNextIdleCallback(
    base::FunctionRef<bool(void)> condition,
    OnceClosure ready_callback) {
  if (condition()) {
    // Invoke `ready_callback` if `condition` evaluates to true.
    std::move(ready_callback).Run();
  } else {
    // Otherwise try again the next time the thread is idle.
    CurrentThread::Get().RegisterOnNextIdleCallback(
        BindOnce(TestPredicateOrRegisterOnNextIdleCallback, condition,
                 std::move(ready_callback)));
  }
}

}  // namespace

bool RunUntil(base::FunctionRef<bool(void)> condition) {
  CHECK(!subtle::ScopedTimeClockOverrides::overrides_active())
      << "Mocked timesource detected, which would cause `RunUntil` to hang "
         "forever on failure.";
  CHECK(test::ScopedRunLoopTimeout::ExistsForCurrentThread())
      << "No RunLoop timeout set, meaning `RunUntil` will hang forever on "
         "failure.";

  test::TestFuture<void> ready_signal;

  CurrentThread::Get().RegisterOnNextIdleCallback(
      BindOnce(TestPredicateOrRegisterOnNextIdleCallback, condition,
               ready_signal.GetCallback()));

  return ready_signal.Wait();
}

}  // namespace base::test
