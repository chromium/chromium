// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"

#include <functional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_future.h"
#include "base/time/time_override.h"

namespace base::test {

void TestPredicateOrRegisterOnNextIdleCallback(
    base::FunctionRef<bool(void)> condition,
    CallbackListSubscription* on_idle_callback_subscription,
    OnceClosure ready_callback) {
  if (condition()) {
    // Invoke `ready_callback` if `condition` evaluates to true.
    std::move(ready_callback).Run();
  } else {
    // Otherwise try again the next time the thread is idle.
    *on_idle_callback_subscription =
        CurrentThread::Get().RegisterOnNextIdleCallback(
            {},
            BindOnce(TestPredicateOrRegisterOnNextIdleCallback, condition,
                     on_idle_callback_subscription, std::move(ready_callback)));
  }
}

bool RunUntil(base::FunctionRef<bool(void)> condition) {
  CHECK(!subtle::ScopedTimeClockOverrides::overrides_active())
      << "Mocked timesource detected, which would cause `RunUntil` to hang "
         "forever on failure.";
  CHECK(test::ScopedRunLoopTimeout::ExistsForCurrentThread())
      << "No RunLoop timeout set, meaning `RunUntil` will hang forever on "
         "failure.";

  test::TestFuture<void> ready_signal;

  CallbackListSubscription on_idle_callback_subscription;
  on_idle_callback_subscription =
      CurrentThread::Get().RegisterOnNextIdleCallback(
          {},
          BindOnce(TestPredicateOrRegisterOnNextIdleCallback, condition,
                   &on_idle_callback_subscription, ready_signal.GetCallback()));

  return ready_signal.Wait();
}

}  // namespace base::test
