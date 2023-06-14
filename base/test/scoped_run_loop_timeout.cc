// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::test {

namespace {

bool g_add_gtest_failure_on_timeout = false;

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
  GTEST_FAIL_AT(run_from_here.file_name(), run_from_here.line_number())
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
    absl::optional<TimeDelta> timeout,
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

  run_timeout_.on_timeout = BindRepeating(
      g_add_gtest_failure_on_timeout ? &TimeoutCallbackWithGtestFailure
                                     : &StandardTimeoutCallback,
      timeout_enabled_from_here, std::move(on_timeout_log));
  RunLoop::SetTimeoutForCurrentThread(&run_timeout_);
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
