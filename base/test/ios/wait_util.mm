// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/timer/elapsed_timer.h"

namespace base::test::ios {

bool WaitUntilConditionOrTimeout(TimeDelta timeout,
                                 bool run_message_loop,
                                 ConditionBlock condition) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeout.InSecondsF()];
  bool success = condition();
  while (!success && [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(kSpinDelaySeconds);
    if (run_message_loop) {
      RunLoop().RunUntilIdle();
    }
    success = condition();
  }
  return success;
}

bool WaitUntilConditionOrTimeout(TimeDelta timeout, ConditionBlock condition) {
  return WaitUntilConditionOrTimeout(timeout, false, condition);
}

void SpinRunLoopWithMaxDelay(TimeDelta max_delay) {
  NSDate* before_date =
      [NSDate dateWithTimeIntervalSinceNow:max_delay.InSecondsF()];
  [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode
                         beforeDate:before_date];
}

void SpinRunLoopWithMinDelay(TimeDelta min_delay) {
  ElapsedTimer timer;
  while (timer.Elapsed() < min_delay) {
    SpinRunLoopWithMaxDelay(Milliseconds(10));
  }
}

}  // namespace base::test::ios
