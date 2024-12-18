// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

bool g_add_gtest_failure_on_timeout = false;

std::unique_ptr<ScopedRunLoopTimeout::TimeoutCallback>
    g_handle_timeout_for_testing = nullptr;

std::string TimeoutMessage(const RepeatingCallback<std::string()>& get_log,
                           const Location& timeout_enabled_from_here) {
  std::string message = "RunLoop::Run() timed out. Timeout set at ";
  message += timeout_enabled_from_here.ToString() + ".";
  if (get_log)
    StrAppend(&message, {"\n", get_log.Run()});
  return message;
}

void StandardTimeoutCallback(const Location& timeout_enabled_from_here,
                             RepeatingCallback<std::string()> on_timeout_log,
                             const Location& run_from_here) {
  const std::string message =
      TimeoutMessage(on_timeout_log, timeout_enabled_from_here);
  logging::LogMessage(run_from_here.file_name(), run_from_here.line_number(),
                      message.data());
}

void TimeoutCallbackWithGtestFailure(
    const Location& timeout_enabled_from_here,
    RepeatingCallback<std::string()> on_timeout_log,
    const Location& run_from_here) {
  // Add a non-fatal failure to GTest result and cause the test to fail.
  // A non-fatal failure is preferred over a fatal one because LUCI Analysis
  // will select the fatal failure over the non-fatal one as the primary error
  // message for the test. The RunLoop::Run() function is generally called by
  // the test framework and generates similar error messages and stack traces,
  // making it difficult to cluster the failures. Making the failure non-fatal
  // will propagate the ASSERT fatal failures in the test body as the primary
  // error message.
  // Also note that, the GTest fatal failure will not actually stop the test
  // execution if not directly used in the test body. A non-fatal/fatal failure
  // here makes no difference to the test running flow.
  ADD_FAILURE_AT(run_from_here.file_name(), run_from_here.line_number())
      << TimeoutMessage(on_timeout_log, timeout_enabled_from_here);
}

}  // namespace

ScopedRunLoopTimeout::ScopedRunLoopTimeout(const Location& from_here,
                                           TimeDelta timeout)
    : ScopedRunLoopTimeout(from_here, timeout, NullCallback()) {}

ScopedRunLoopTimeout::~ScopedRunLoopTimeout() {
  // Out-of-order destruction could result in UAF.
  CHECK_EQ(&run_timeout_, RunLoop::GetTimeoutForCurrentThread());
  RunLoop::SetTimeoutForCurrentThread(nested_timeout_);
}

ScopedRunLoopTimeout::ScopedRunLoopTimeout(
    const Location& timeout_enabled_from_here,
    std::optional<TimeDelta> timeout,
    RepeatingCallback<std::string()> on_timeout_log)
    : nested_timeout_(RunLoop::GetTimeoutForCurrentThread()) {
  CHECK(timeout.has_value() || nested_timeout_)
      << "Cannot use default timeout if no default is set.";
  // We can't use value_or() here because it gets the value in parentheses no
  // matter timeout has a value or not, causing null pointer dereference if
  // nested_timeout_ is nullptr.
  run_timeout_.timeout =
      timeout.has_value() ? timeout.value() : nested_timeout_->timeout;
  CHECK_GT(run_timeout_.timeout, TimeDelta());

  run_timeout_.on_timeout =
      BindRepeating(GetTimeoutCallback(), timeout_enabled_from_here,
                    std::move(on_timeout_log));

  RunLoop::SetTimeoutForCurrentThread(&run_timeout_);
}

ScopedRunLoopTimeout::TimeoutCallback
ScopedRunLoopTimeout::GetTimeoutCallback() {
  // In case both g_handle_timeout_for_testing and
  // g_add_gtest_failure_on_timeout are set, we chain the callbacks so that they
  // both get called eventually. This avoids confusion on what exactly is
  // happening, especially for tests that are not controlling the call to
  // `SetAddGTestFailureOnTimeout` directly.
  if (g_handle_timeout_for_testing) {
    if (g_add_gtest_failure_on_timeout) {
      return ForwardRepeatingCallbacks(
          {BindRepeating(&TimeoutCallbackWithGtestFailure),
           *g_handle_timeout_for_testing});
    }
    return *g_handle_timeout_for_testing;
  } else if (g_add_gtest_failure_on_timeout) {
    return BindRepeating(&TimeoutCallbackWithGtestFailure);
  } else {
    return BindRepeating(&StandardTimeoutCallback);
  }
}

// static
bool ScopedRunLoopTimeout::ExistsForCurrentThread() {
  return RunLoop::GetTimeoutForCurrentThread() != nullptr;
}

// static
void ScopedRunLoopTimeout::SetAddGTestFailureOnTimeout() {
  g_add_gtest_failure_on_timeout = true;
}

// static
const RunLoop::RunLoopTimeout*
ScopedRunLoopTimeout::GetTimeoutForCurrentThread() {
  return RunLoop::GetTimeoutForCurrentThread();
}

// static
void ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(
    std::unique_ptr<ScopedRunLoopTimeout::TimeoutCallback> cb) {
  g_handle_timeout_for_testing = std::move(cb);
}

ScopedDisableRunLoopTimeout::ScopedDisableRunLoopTimeout()
    : nested_timeout_(RunLoop::GetTimeoutForCurrentThread()) {
  RunLoop::SetTimeoutForCurrentThread(nullptr);
}

ScopedDisableRunLoopTimeout::~ScopedDisableRunLoopTimeout() {
  // Out-of-order destruction could result in UAF.
  CHECK_EQ(nullptr, RunLoop::GetTimeoutForCurrentThread());
  RunLoop::SetTimeoutForCurrentThread(nested_timeout_);
}

}  // namespace base::test
