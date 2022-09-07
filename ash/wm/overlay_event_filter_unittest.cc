// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overlay_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/test_overlay_delegate.h"

namespace ash {

using OverlayEventFilterTest = AshTestBase;

// Tests of the multiple overlay delegates attempt to activate, in that case
// Cancel() of the existing delegate should be called.
// See http://crbug.com/341958
TEST_F(OverlayEventFilterTest, CancelAtActivating) {
  TestOverlayDelegate d1;
  TestOverlayDelegate d2;

  Shell::Get()->overlay_filter()->Activate(&d1);
  EXPECT_EQ(0, d1.GetCancelCountAndReset());
  EXPECT_EQ(0, d2.GetCancelCountAndReset());

  Shell::Get()->overlay_filter()->Activate(&d2);
  EXPECT_EQ(1, d1.GetCancelCountAndReset());
  EXPECT_EQ(0, d2.GetCancelCountAndReset());

  Shell::Get()->overlay_filter()->Cancel();
  EXPECT_EQ(0, d1.GetCancelCountAndReset());
  EXPECT_EQ(1, d2.GetCancelCountAndReset());
}

}  // namespace ash
