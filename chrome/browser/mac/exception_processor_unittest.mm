// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/exception_processor.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <sys/wait.h>

#include "base/mac/os_crash_dumps.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExceptionProcessorTest : public testing::Test {
 public:
  ExceptionProcessorTest() {
    features_.InitWithFeatures({kForceCrashOnExceptions}, {});
  }

 protected:
  base::test::ScopedFeatureList features_;
};

void RaiseExceptionInRunLoop() {
  CFRunLoopRef run_loop = CFRunLoopGetCurrent();

  CFRunLoopPerformBlock(run_loop, kCFRunLoopCommonModes, ^{
    [NSException raise:@"ThrowExceptionInRunLoop" format:@""];
  });
  CFRunLoopPerformBlock(run_loop, kCFRunLoopCommonModes, ^{
    CFRunLoopStop(run_loop);
  });
  CFRunLoopRun();
}

void ThrowExceptionInRunLoop() {
  base::mac::DisableOSCrashDumps();
  InstallObjcExceptionPreprocessor();

  RaiseExceptionInRunLoop();

  fprintf(stderr, "TEST FAILED\n");
  exit(1);
}

// Tests that when the preprocessor is installed, exceptions thrown from
// a runloop callout are made fatal, so that the stack trace is useful.
TEST_F(ExceptionProcessorTest, ThrowExceptionInRunLoop) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(ThrowExceptionInRunLoop(),
               ".*FATAL:exception_processor\\.mm.*"
               "Terminating from Objective-C exception:.*");
}

void ThrowAndCatchExceptionInRunLoop() {
  base::mac::DisableOSCrashDumps();
  InstallObjcExceptionPreprocessor();

  CFRunLoopRef run_loop = CFRunLoopGetCurrent();
  CFRunLoopPerformBlock(run_loop, kCFRunLoopCommonModes, ^{
    @try {
      [NSException raise:@"ObjcExceptionPreprocessCaught" format:@""];
    } @catch (id exception) {
    }
  });

  CFRunLoopPerformBlock(run_loop, kCFRunLoopCommonModes, ^{
    CFRunLoopStop(run_loop);
  });

  CFRunLoopRun();

  fprintf(stderr, "TEST PASS\n");
  exit(0);
}

// Tests that exceptions can still be caught when the preprocessor is enabled.
TEST_F(ExceptionProcessorTest, ThrowAndCatchExceptionInRunLoop) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(ThrowAndCatchExceptionInRunLoop(),
              [](int exit_code) -> bool {
                return WEXITSTATUS(exit_code) == 0;
              },
              ".*TEST PASS.*");
}

void ThrowExceptionFromSelector() {
  base::mac::DisableOSCrashDumps();
  InstallObjcExceptionPreprocessor();

  NSException* exception = [NSException exceptionWithName:@"ThrowFromSelector"
                                                   reason:@""
                                                 userInfo:nil];

  [exception performSelector:@selector(raise) withObject:nil afterDelay:0.1];

  [[NSRunLoop currentRunLoop] runUntilDate:
      [NSDate dateWithTimeIntervalSinceNow:10]];

  fprintf(stderr, "TEST FAILED\n");
  exit(1);
}

TEST_F(ExceptionProcessorTest, ThrowExceptionFromSelector) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(ThrowExceptionFromSelector(),
               ".*FATAL:exception_processor\\.mm.*"
               "Terminating from Objective-C exception:.*");
}

void ThrowInNotificationObserver() {
  base::mac::DisableOSCrashDumps();
  InstallObjcExceptionPreprocessor();

  NSNotification* notification =
      [NSNotification notificationWithName:@"TestExceptionInObserver"
                                    object:nil];

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserverForName:[notification name]
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification*) {
                    [NSException raise:@"ThrowInNotificationObserver"
                                format:@""];
                  }];

  [center performSelector:@selector(postNotification:)
               withObject:notification
               afterDelay:0];

  [[NSRunLoop currentRunLoop] runUntilDate:
      [NSDate dateWithTimeIntervalSinceNow:10]];

  fprintf(stderr, "TEST FAILED\n");
  exit(1);
}

TEST_F(ExceptionProcessorTest, ThrowInNotificationObserver) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(ThrowInNotificationObserver(),
               ".*FATAL:exception_processor\\.mm.*"
               "Terminating from Objective-C exception:.*");
}

void ThrowExceptionInRunLoopWithoutProcessor() {
  base::mac::DisableOSCrashDumps();
  UninstallObjcExceptionPreprocessor();

  @try {
    RaiseExceptionInRunLoop();
  } @catch (id exception) {
    fprintf(stderr, "TEST PASS\n");
    exit(0);
  }

  fprintf(stderr, "TEST FAILED\n");
  exit(1);
}

// Under LSAN this dies from leaking the run loop instead of how we expect it to
// die, so the exit code is wrong.
#if defined(LEAK_SANITIZER)
#define MAYBE_ThrowExceptionInRunLoopWithoutProcessor \
  DISABLED_ThrowExceptionInRunLoopWithoutProcessor
#else
#define MAYBE_ThrowExceptionInRunLoopWithoutProcessor \
  ThrowExceptionInRunLoopWithoutProcessor
#endif
// Tests basic exception handling when the preprocessor is disabled.
TEST_F(ExceptionProcessorTest, MAYBE_ThrowExceptionInRunLoopWithoutProcessor) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(ThrowExceptionInRunLoopWithoutProcessor(),
              [](int exit_code) -> bool {
                return WEXITSTATUS(exit_code) == 0;
              },
              ".*TEST PASS.*");
}
