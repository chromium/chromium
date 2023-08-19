// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/google_test_runner_delegate.h"

@interface GoogleTestRunner : XCTestCase
@end

@implementation GoogleTestRunner

- (void)testRunGoogleTests {
  self.continueAfterFailure = false;

  id appDelegate = UIApplication.sharedApplication.delegate;
  XCTAssertTrue(
      [appDelegate conformsToProtocol:@protocol(GoogleTestRunnerDelegate)]);

  id<GoogleTestRunnerDelegate> runnerDelegate =
      static_cast<id<GoogleTestRunnerDelegate>>(appDelegate);
  XCTAssertTrue(runnerDelegate.supportsRunningGoogleTests);
  XCTAssertTrue([runnerDelegate runGoogleTests] == 0);
}

@end
