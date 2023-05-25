// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/call_with_eh_frame.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base::mac {
namespace {

class CallWithEHFrameTest : public testing::Test {
 protected:
  void ThrowException() {
    @throw [NSException exceptionWithName:@"TestException"
                                   reason:@"Testing exceptions"
                                 userInfo:nil];
  }
};

// Catching from within the EHFrame is allowed.
TEST_F(CallWithEHFrameTest, CatchExceptionHigher) {
  bool __block saw_exception = false;
  base::mac::CallWithEHFrame(^{
    @try {
      ThrowException();
    } @catch (NSException* exception) {
      saw_exception = true;
    }
  });
  EXPECT_TRUE(saw_exception);
}

// Trying to catch an exception outside the EHFrame is blocked.
TEST_F(CallWithEHFrameTest, CatchExceptionLower) {
  auto catch_exception_lower = ^{
    bool saw_exception = false;
    @try {
      base::mac::CallWithEHFrame(^{
        ThrowException();
      });
    } @catch (NSException* exception) {
      saw_exception = true;
    }
    ASSERT_FALSE(saw_exception);
  };
  EXPECT_DEATH(catch_exception_lower(), "");
}

}  // namespace
}  // namespace base::mac
