// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

using MainThreadScrollingReasonTest = testing::Test;

TEST_F(MainThreadScrollingReasonTest, AsText) {
  EXPECT_EQ("", MainThreadScrollingReason::AsText(MainThreadRepaintReasons{}));
  EXPECT_EQ(
      "Has background-attachment:fixed, "
      "Not opaque for text and LCD text, "
      "Prefer non-composited scrolling, "
      "Background needs repaint on scroll",
      MainThreadScrollingReason::AsText(MainThreadRepaintReasons::All()));
  EXPECT_EQ(
      "Scrollbar scrolling, "
      "Main thread scroll hit test region, "
      "Failed hit test",
      MainThreadScrollingReason::AsText(MainThreadHitTestReasons::All()));

  EXPECT_EQ(
      "Popup scrolling (no threaded input handler), "
      "Wheel event handler region, "
      "Touch event handler region",
      MainThreadScrollingReason::AsText(
          MainThreadScrollingOtherReasons::All()));
}

}  // namespace cc
