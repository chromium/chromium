// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_RUN_LOOP_TIMEOUT_H_
#define BASE_TEST_SCOPED_RUN_LOOP_TIMEOUT_H_

#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/time/time.h"

namespace content {
FORWARD_DECLARE_TEST(ContentBrowserTest, RunTimeoutInstalled);
}

namespace base {
namespace test {

FORWARD_DECLARE_TEST(TaskEnvironmentTest, SetsDefaultRunTimeout);

// Configures all RunLoop::Run() calls on the current thread to run the
// supplied |on_timeout| callback if they run for longer than |timeout|.
//
// Specifying Run() timeouts per-thread avoids the need to cope with Run()s
// executing concurrently with ScopedRunLoopTimeout initialization or
// teardown, and allows "default" timeouts to be specified by suites, rather
// than explicitly configuring them for every RunLoop, in each test.
//
// This is used by test classes including TaskEnvironment and TestSuite to
// set a default Run() timeout on the main thread of all tests which use them.
//
// Tests which have steps which need to Run() for longer than their suite's
// default (if any) allows can override the active timeout by creating a nested
// ScopedRunLoopTimeout on their stack, e.g:
//
//   ScopedRunLoopTimeout default_timeout(kDefaultRunTimeout);
//   ... do other test stuff ...
//   RunLoop().Run(); // Run for up to kDefaultRunTimeout.
//   ...
//   {
//     ScopedRunLoopTimeout specific_timeout(kTestSpecificTimeout);
//     RunLoop().Run(); // Run for up to kTestSpecificTimeout.
//   }
//   ...
//   RunLoop().Run(); // Run for up to kDefaultRunTimeout.
//
// The currently-active timeout can also be temporarily disabled:
//   ScopedDisableRunLoopTimeout disable_timeout;
//
// By default LOG(FATAL) will be invoked on Run() timeout. Test binaries
// can opt-in to using ADD_FAILURE() instead by calling
// SetAddGTestFailureOnTimeout() during process initialization.
//
// TaskEnvironment applies a default Run() timeout.

class ScopedRunLoopTimeout {
 public:
  ScopedRunLoopTimeout(const Location& from_here, TimeDelta timeout);
  ~ScopedRunLoopTimeout();

  // Invokes |on_timeout_log| if |timeout| expires, and appends it to the
  // logged error message.
  ScopedRunLoopTimeout(const Location& from_here,
                       TimeDelta timeout,
                       RepeatingCallback<std::string()> on_timeout_log);

  ScopedRunLoopTimeout(const ScopedRunLoopTimeout&) = delete;
  ScopedRunLoopTimeout& operator=(const ScopedRunLoopTimeout&) = delete;

  // Returns true if there is a Run() timeout configured on the current thread.
  static bool ExistsForCurrentThread();

  static void SetAddGTestFailureOnTimeout();

 protected:
  FRIEND_TEST_ALL_PREFIXES(ScopedRunLoopRunTimeoutTest, TimesOut);
  FRIEND_TEST_ALL_PREFIXES(ScopedRunLoopRunTimeoutTest, RunTasksUntilTimeout);
  FRIEND_TEST_ALL_PREFIXES(TaskEnvironmentTest, SetsDefaultRunTimeout);
  FRIEND_TEST_ALL_PREFIXES(content::ContentBrowserTest, RunTimeoutInstalled);

  // Exposes the RunLoopTimeout to the friend tests (see above).
  static const RunLoop::RunLoopTimeout* GetTimeoutForCurrentThread();

  const RunLoop::RunLoopTimeout* const nested_timeout_;
  RunLoop::RunLoopTimeout run_timeout_;
};

class ScopedDisableRunLoopTimeout {
 public:
  ScopedDisableRunLoopTimeout();
  ~ScopedDisableRunLoopTimeout();

  ScopedDisableRunLoopTimeout(const ScopedDisableRunLoopTimeout&) = delete;
  ScopedDisableRunLoopTimeout& operator=(const ScopedDisableRunLoopTimeout&) =
      delete;

 private:
  const RunLoop::RunLoopTimeout* const nested_timeout_;
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_SCOPED_RUN_LOOP_TIMEOUT_H_
