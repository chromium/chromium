// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_

#include <iosfwd>

#include "base/run_loop.h"
#include "base/time/time.h"

namespace switches {

inline constexpr char kStatusChangeCheckerTimeoutInSeconds[] =
    "sync-status-change-checker-timeout";

}  // namespace switches

// Interface for a helper class that can pump the message loop while waiting
// for a certain state transition to take place.
//
// This is a template that should be filled in by child classes so they can
// observe specific kinds of changes and await specific conditions.
//
// The instances of this class are intended to be single-use.  It doesn't make
// sense to call StartBlockingWait() more than once.
//
// |switches::kStatusChangeCheckerTimeoutInSeconds| can be passed to the command
// line to override the timeout used by instances of this class.
class StatusChangeChecker {
 public:
  StatusChangeChecker();

  // Block if IsExitConditionSatisfied() is currently false until TimedOut()
  // becomes true. Checkers should call CheckExitCondition upon changes, which
  // can cause Wait() to immediately return true if IsExitConditionSatisfied(),
  // and continue to block if not. Returns false if and only if timeout occurs.
  virtual bool Wait();

  // Returns true if the blocking wait was exited because of a timeout.
  bool TimedOut() const;

 protected:
  virtual ~StatusChangeChecker();

  // Returns whether the state the checker is currently in is its desired
  // configuration. |os| must not be null and allows subclasses to provide
  // details about why the condition was not satisfied. |os| must not be null.
  virtual bool IsExitConditionSatisfied(std::ostream* os) = 0;

  // Checks IsExitConditionSatisfied() and calls StopWaiting() if it returns
  // true.
  virtual void CheckExitCondition();

  // Called when Wait() is done, i.e. from StopWaiting(). Subclasses can
  // override this to capture state at exactly the time that the exit condition
  // is satisfied. This does *not* get called in the timeout case.
  // If the exit condition is already satisfied at the time Wait() is called,
  // then this is called immediately.
  virtual void WaitDone() {}

 private:
  // Helper function to start running the nested run loop (run_loop_).
  //
  // Will exit if IsExitConditionSatisfied() returns true when called from
  // CheckExitCondition(), if a timeout occurs, or if StopWaiting() is called.
  //
  // The timeout length is specified with GetTimeoutDuration().
  void StartBlockingWait();

  // Stop the nested running of the message loop started in StartBlockingWait().
  void StopWaiting();

  // Called when the blocking wait timeout is exceeded.
  void OnTimeout();

  const base::TimeDelta timeout_;
  base::RunLoop run_loop_;
  bool timed_out_ = false;

  bool wait_done_called_ = false;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
