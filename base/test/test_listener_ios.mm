// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_listener_ios.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

// The iOS watchdog timer will kill an app that doesn't spin the main event
// loop often enough. This uses a Gtest TestEventListener to spin the current
// loop after each test finishes. However, if any individual test takes too
// long, it is still possible that the app will get killed.

namespace {

// Clears the app's persistent `NSUserDefaults` domain when non-empty. Skipping
// removal when already empty avoids unnecessary `cfprefsd` writes and
// `NSUserDefaultsDidChangeNotification` broadcasts between tests.
void ClearStandardUserDefaults() {
  NSString* bundle_id = [NSBundle mainBundle].bundleIdentifier;
  if (!bundle_id) {
    return;
  }
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults persistentDomainForName:bundle_id].count > 0) {
    [defaults removePersistentDomainForName:bundle_id];
  }
}

class IOSRunLoopListener : public testing::EmptyTestEventListener {
 public:
  void OnTestProgramStart(const testing::UnitTest& unit_test) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;
};

void IOSRunLoopListener::OnTestProgramStart(
    const testing::UnitTest& unit_test) {
  @autoreleasepool {
    // Clear stale NSUserDefaults left by a prior run on a reused simulator
    // (e.g. after a crash or watchdog kill that skipped OnTestEnd).
    ClearStandardUserDefaults();
  }
}

void IOSRunLoopListener::OnTestEnd(const testing::TestInfo& test_info) {
  @autoreleasepool {
    // At the end of the test, spin the default loop for a moment.
    NSDate* stop_date = [NSDate dateWithTimeIntervalSinceNow:0.001];
    [NSRunLoop.currentRunLoop runUntilDate:stop_date];

    // Clear NSUserDefaults to prevent state leakage.
    ClearStandardUserDefaults();
  }
}

}  // namespace

namespace base::test_listener_ios {

void RegisterTestEndListener() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new IOSRunLoopListener);
}

}  // namespace base::test_listener_ios
