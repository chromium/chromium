// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/status_area_overflow_button_tray.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"
#include "ui/views/accessibility/view_accessibility.h"

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

TEST_F(StatusAreaOverflowButtonTrayTest, AccessibleName) {
  auto* overflow_button_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->overflow_button_tray();
  overflow_button_tray->SetVisiblePreferred(true);

  // Test that the accessible name matches the `CLICK_TO_EXPAND` state at
  // construction.
  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_EXPAND,
            overflow_button_tray->state());
  ui::AXNodeData node_data;
  overflow_button_tray->GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_EQ(
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_EXPAND));

  GestureTapOn(overflow_button_tray);

  // Test that the accessible name gets updated when the state gets updated.
  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_COLLAPSE,
            overflow_button_tray->state());
  node_data = ui::AXNodeData();
  overflow_button_tray->GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_EQ(
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_COLLAPSE));
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
