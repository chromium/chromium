// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_sending_event.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

#ifdef LEAK_SANITIZER
#include <sanitizer/lsan_interface.h>
#endif

@interface ScopedSendingEventTestCrApp : NSApplication <CrAppControlProtocol> {
 @private
  BOOL _handlingSendEvent;
}
@property(nonatomic, assign, getter=isHandlingSendEvent) BOOL handlingSendEvent;
@end

@implementation ScopedSendingEventTestCrApp
@synthesize handlingSendEvent = _handlingSendEvent;
@end

namespace {

class ScopedSendingEventTest : public testing::Test {
 public:
  ScopedSendingEventTest() {
#ifdef LEAK_SANITIZER
    // NSApplication's `init` creates a helper object and writes it to an
    // AppKit-owned static unconditionally. This is not cleaned up on
    // NSApplication dealloc.
    // When we create a new NSApplication, as we do when we run multiple
    // tests in this suite, a new object is created and stomps on the old
    // static.
    // This needs a scoped disabler instead of just ignoring the app
    // object since the leak is a side-effect of object creation.
    __lsan::ScopedDisabler disable;
#endif
    app_ = [[ScopedSendingEventTestCrApp alloc] init];
    NSApp = app_;
  }
  ~ScopedSendingEventTest() override { NSApp = nil; }

 private:
  ScopedSendingEventTestCrApp* __strong app_;
};

// Sets the flag within scope, resets when leaving scope.
TEST_F(ScopedSendingEventTest, SetHandlingSendEvent) {
  id<CrAppProtocol> app = NSApp;
  EXPECT_FALSE([app isHandlingSendEvent]);
  {
    base::mac::ScopedSendingEvent is_handling_send_event;
    EXPECT_TRUE([app isHandlingSendEvent]);
  }
  EXPECT_FALSE([app isHandlingSendEvent]);
}

// Nested call restores previous value rather than resetting flag.
TEST_F(ScopedSendingEventTest, NestedSetHandlingSendEvent) {
  id<CrAppProtocol> app = NSApp;
  EXPECT_FALSE([app isHandlingSendEvent]);
  {
    base::mac::ScopedSendingEvent is_handling_send_event;
    EXPECT_TRUE([app isHandlingSendEvent]);
    {
      base::mac::ScopedSendingEvent nested_is_handling_send_event;
      EXPECT_TRUE([app isHandlingSendEvent]);
    }
    EXPECT_TRUE([app isHandlingSendEvent]);
  }
  EXPECT_FALSE([app isHandlingSendEvent]);
}

}  // namespace
