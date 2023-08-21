// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/status_area_overflow_button_tray.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"

namespace ash {

using StatusAreaOverflowButtonTrayTest = AshTestBase;

// Tests that the button reacts to press as expected. Artificially sets the
// button to be visible.
TEST_F(StatusAreaOverflowButtonTrayTest, ToggleExpanded) {
  auto* overflow_button_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->overflow_button_tray();
  overflow_button_tray->SetVisiblePreferred(true);

  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_EXPAND,
            overflow_button_tray->state());

  GestureTapOn(overflow_button_tray);

  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_COLLAPSE,
            overflow_button_tray->state());

  // Force the tray button to be visible. It is currently not visible because
  // tablet mode is not enabled.
  overflow_button_tray->SetVisiblePreferred(true);
  GestureTapOn(overflow_button_tray);

  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_EXPAND,
            overflow_button_tray->state());
}

TEST_F(StatusAreaOverflowButtonTrayTest, UMATracking) {
  // No metrics logged before showing the tray.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.StatusArea.TrayBackgroundView.Shown",
                                     /*count=*/0);

  auto* overflow_button_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->overflow_button_tray();
  overflow_button_tray->SetVisiblePreferred(true);

  histogram_tester->ExpectTotalCount("Ash.StatusArea.TrayBackgroundView.Shown",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.StatusArea.TrayBackgroundView.Shown",
      TrayBackgroundViewCatalogName::kStatusAreaOverflowButton,
      /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.StatusArea.TrayBackgroundView.Hidden",
      TrayBackgroundViewCatalogName::kStatusAreaOverflowButton,
      /*expected_count=*/0);

  overflow_button_tray->SetVisiblePreferred(false);
  histogram_tester->ExpectTotalCount("Ash.StatusArea.TrayBackgroundView.Shown",
                                     /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.StatusArea.TrayBackgroundView.Hidden",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.StatusArea.TrayBackgroundView.Shown",
      TrayBackgroundViewCatalogName::kStatusAreaOverflowButton,
      /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.StatusArea.TrayBackgroundView.Hidden",
      TrayBackgroundViewCatalogName::kStatusAreaOverflowButton,
      /*expected_count=*/1);
}

}  // namespace ash
