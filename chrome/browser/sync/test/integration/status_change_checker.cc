// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/status_change_checker.h"

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kDefaultTimeout = base::Seconds(30);

base::TimeDelta GetTimeoutFromCommandLineOrDefault() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStatusChangeCheckerTimeoutInSeconds)) {
    return kDefaultTimeout;
  }
  std::string timeout_string(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kStatusChangeCheckerTimeoutInSeconds));
  int timeout_in_seconds = 0;
  if (!base::StringToInt(timeout_string, &timeout_in_seconds)) {
    LOG(FATAL) << "Timeout value \"" << timeout_string << "\" was parsed as "
               << timeout_in_seconds;
  }
  return base::Seconds(timeout_in_seconds);
}

}  // namespace

StatusChangeChecker::StatusChangeChecker()
    : timeout_(GetTimeoutFromCommandLineOrDefault()),
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {}

StatusChangeChecker::~StatusChangeChecker() = default;

bool StatusChangeChecker::Wait() {
  std::ostringstream s;
  if (IsExitConditionSatisfied(&s)) {
    DVLOG(1) << "Already satisfied: " << s.str();
    wait_done_called_ = true;
    WaitDone();
  } else {
    DVLOG(1) << "Blocking: " << s.str();
    StartBlockingWait();
  }
  return !TimedOut();
}

bool StatusChangeChecker::TimedOut() const {
  return timed_out_;
}

void StatusChangeChecker::StopWaiting() {
  if (run_loop_.running()) {
    // Note that we can get here multiple times in some situations, because
    // RunLoop::Quit() doesn't guarantee that it actually quits immediately.
    // Make sure that WaitDone() still gets called only once.
    if (!wait_done_called_) {
      wait_done_called_ = true;
      WaitDone();
    }
    run_loop_.Quit();
  }
}

void StatusChangeChecker::CheckExitCondition() {
  if (!run_loop_.running()) {
    return;
  }

  std::ostringstream s;
  if (IsExitConditionSatisfied(&s)) {
    DVLOG(1) << "Await -> Condition met: " << s.str();
    StopWaiting();
  } else {
    DVLOG(1) << "Await -> Condition not met: " << s.str();
  }
}

void StatusChangeChecker::StartBlockingWait() {
  DCHECK(!run_loop_.running());

  base::OneShotTimer timer;
  timer.Start(
      FROM_HERE, timeout_,
      base::BindOnce(&StatusChangeChecker::OnTimeout, base::Unretained(this)));

  run_loop_.Run();
}

void StatusChangeChecker::OnTimeout() {
  timed_out_ = true;

  std::ostringstream s;
  if (IsExitConditionSatisfied(&s)) {
    ADD_FAILURE() << "Await -> Timed out despite conditions being satisfied.";
  } else {
    ADD_FAILURE() << "Await -> Timed out: " << s.str();
  }

  StopWaiting();
}
