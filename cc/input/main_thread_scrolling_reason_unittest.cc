// Copyright 2018 The Chromium Authors. All rights reserved.
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
      "Has non-layer viewport-constrained objects, "
      "Threaded scrolling is disabled, "
      "Scrollbar scrolling, "
      "Frame overlay, "
      "Handling scroll from main thread, "
      "Has opacity and LCD text, "
      "Has transform and LCD text, "
      "Background is not opaque in rect and LCD text, "
      "Has clip related property, "
      "Has box shadow from non-root layer, "
      "Is not stacking context and LCD text, "
      "Non fast scrollable region, "
      "Failed hit test, "
      "No scrolling layer, "
      "Not scrollable, "
      "Continuing main thread scroll, "
      "Non-invertible transform, "
      "Page-based scrolling, "
      "Wheel event handler region, "
      "Touch event handler region",
      MainThreadScrollingReason::AsText(0xffffffffu));
}

}  // namespace cc
