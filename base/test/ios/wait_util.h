// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_IOS_WAIT_UTIL_H_
#define BASE_TEST_IOS_WAIT_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#include "base/time/time.h"

namespace base {
namespace test {
namespace ios {

// Constant for UI wait loop in seconds.
extern const NSTimeInterval kSpinDelaySeconds;

// Constant for timeout in seconds while waiting for UI element.
extern const NSTimeInterval kWaitForUIElementTimeout;

// Constant for timeout in seconds while waiting for JavaScript completion.
extern const NSTimeInterval kWaitForJSCompletionTimeout;

// Constant for timeout in seconds while waiting for a download to complete.
extern const NSTimeInterval kWaitForDownloadTimeout;

// Constant for timeout in seconds while waiting for a pageload to complete.
extern const NSTimeInterval kWaitForPageLoadTimeout;

// Constant for timeout in seconds while waiting for a generic action to
// complete.
extern const NSTimeInterval kWaitForActionTimeout;

// Constant for timeout in seconds while waiting for clear browsing data. It
// seems this can take a very long time on the bots when running simulators in
// parallel. TODO(crbug.com/993513): Investigate why this is sometimes very
// slow.
extern const NSTimeInterval kWaitForClearBrowsingDataTimeout;

// Constant for timeout in seconds while waiting for cookies operations to
// complete.
extern const NSTimeInterval kWaitForCookiesTimeout;

// Constant for timeout in seconds while waiting for a file operation to
// complete.
extern const NSTimeInterval kWaitForFileOperationTimeout;

// Returns true when condition() becomes true, otherwise returns false after
// |timeout|.
bool WaitUntilConditionOrTimeout(NSTimeInterval timeout,
                                 ConditionBlock condition) WARN_UNUSED_RESULT;

// Runs |action| if non-nil. Then, until either |condition| is true or |timeout|
// expires, repetitively runs the current NSRunLoop and the current MessageLoop
// (if |run_message_loop| is true). |condition| may be nil if there is no
// condition to wait for; the NSRunLoop and current MessageLoop will be run run
// until |timeout| expires. DCHECKs if |condition| is non-nil and |timeout|
// expires before |condition| becomes true. If |timeout| is zero, a reasonable
// default is used. Returns the time spent in the function.
// DEPRECATED - Do not use in new code. http://crbug.com/784735
TimeDelta TimeUntilCondition(ProceduralBlock action,
                             ConditionBlock condition,
                             bool run_message_loop,
                             TimeDelta timeout);

// Same as TimeUntilCondition, but doesn't run an action.
// DEPRECATED - Do not use in new code. http://crbug.com/784735
void WaitUntilCondition(ConditionBlock condition,
                        bool run_message_loop,
                        TimeDelta timeout);
// DEPRECATED - Do not use in new code. http://crbug.com/784735
void WaitUntilCondition(ConditionBlock condition);

// Lets the run loop of the current thread process other messages
// within the given maximum delay. This method may return before max_delay
// elapsed.
void SpinRunLoopWithMaxDelay(TimeDelta max_delay);

// Lets the run loop of the current thread process other messages
// within the given minimum delay. This method returns after |min_delay|
// elapsed.
void SpinRunLoopWithMinDelay(TimeDelta min_delay);

}  // namespace ios
}  // namespace test
}  // namespace base

#endif  // BASE_TEST_IOS_WAIT_UTIL_H_
