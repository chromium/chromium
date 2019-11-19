// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_

#include <iosfwd>

#include "base/run_loop.h"

// Interface for a helper class that can pump the message loop while waiting
// for a certain state transition to take place.
//
// This is a template that should be filled in by child classes so they can
// observe specific kinds of changes and await specific conditions.
//
// The instances of this class are intended to be single-use.  It doesn't make
// sense to call StartBlockingWait() more than once.
class StatusChangeChecker {
 public:
  StatusChangeChecker();

  // Returns whether the state the checker is currently in is its desired
  // configuration. |os| must not be null and allows subclasses to provide
  // details about why the condition was not satisfied. |os| must not be null.
  virtual bool IsExitConditionSatisfied(std::ostream* os) = 0;

  // Block if IsExitConditionSatisfied() is currently false until TimedOut()
  // becomes true. Checkers should call CheckExitCondition upon changes, which
  // can cause Wait() to immediately return true if IsExitConditionSatisfied(),
  // and continue to block if not. Returns false if and only if timeout occurs.
  virtual bool Wait();

  // Returns true if the blocking wait was exited because of a timeout.
  bool TimedOut() const;

 protected:
  virtual ~StatusChangeChecker();

  // Stop the nested running of the message loop started in StartBlockingWait().
  void StopWaiting();

  // Checks IsExitConditionSatisfied() and calls StopWaiting() if it returns
  // true.
  virtual void CheckExitCondition();

 private:
  // Helper function to start running the nested run loop (run_loop_).
  //
  // Will exit if IsExitConditionSatisfied() returns true when called from
  // CheckExitCondition(), if a timeout occurs, or if StopWaiting() is called.
  //
  // The timeout length is specified with GetTimeoutDuration().
  void StartBlockingWait();

  // Called when the blocking wait timeout is exceeded.
  void OnTimeout();

  base::RunLoop run_loop_;
  bool timed_out_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
