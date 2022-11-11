// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/timer/elapsed_timer.h"

namespace base {
namespace test {
namespace ios {

bool WaitUntilConditionOrTimeout(TimeDelta timeout, ConditionBlock condition) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeout.InSecondsF()];
  bool success = condition();
  while (!success && [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(kSpinDelaySeconds);
    success = condition();
  }
  return success;
}

TimeDelta TimeUntilCondition(ProceduralBlock action,
                             ConditionBlock condition,
                             bool run_message_loop,
                             TimeDelta timeout) {
  ElapsedTimer timer;
  if (action)
    action();
  if (timeout.is_zero())
    timeout = TestTimeouts::action_timeout();
  constexpr TimeDelta kSpinDelay(Milliseconds(10));
  bool condition_evaluation_result = false;
  while (timer.Elapsed() < timeout &&
         (!condition || !(condition_evaluation_result = condition()))) {
    SpinRunLoopWithMaxDelay(kSpinDelay);
    if (run_message_loop)
      RunLoop().RunUntilIdle();
  }
  const TimeDelta elapsed = timer.Elapsed();
  // If DCHECK is ever hit, check if |action| is doing something that is
  // taking an unreasonably long time, or if |condition| does not come
  // true quickly enough. Increase |timeout| only if necessary.
  DCHECK(!condition || condition_evaluation_result);
  return elapsed;
}

void WaitUntilCondition(ConditionBlock condition,
                        bool run_message_loop,
                        TimeDelta timeout) {
  TimeUntilCondition(nil, condition, run_message_loop, timeout);
}

void WaitUntilCondition(ConditionBlock condition) {
  WaitUntilCondition(condition, false, TimeDelta());
}

void SpinRunLoopWithMaxDelay(TimeDelta max_delay) {
  scoped_nsobject<NSDate> beforeDate(
      [[NSDate alloc] initWithTimeIntervalSinceNow:max_delay.InSecondsF()]);
  [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                           beforeDate:beforeDate];
}

void SpinRunLoopWithMinDelay(TimeDelta min_delay) {
  ElapsedTimer timer;
  while (timer.Elapsed() < min_delay) {
    SpinRunLoopWithMaxDelay(Milliseconds(10));
  }
}

}  // namespace ios
}  // namespace test
}  // namespace base
