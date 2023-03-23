// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

using MainThreadScrollingReasonTest = testing::Test;

TEST_F(MainThreadScrollingReasonTest, AsText) {
  EXPECT_EQ("", MainThreadScrollingReason::AsText(0));
  EXPECT_EQ(
      "Has background-attachment:fixed, "
      "Threaded scrolling is disabled, "
      "Scrollbar scrolling, "
      "Not opaque for text and LCD text, "
      "Can't paint scrolling background and LCD text, "
      "Non fast scrollable region, "
      "Failed hit test, "
      "No scrolling layer, "
      "Wheel event handler region, "
      "Touch event handler region",
      MainThreadScrollingReason::AsText(0xffffffffu));
}

}  // namespace cc
