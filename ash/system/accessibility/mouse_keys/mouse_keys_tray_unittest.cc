// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/base_event_utils.h"

namespace ash {

namespace {

MouseKeysTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->mouse_keys_tray();
}

// Returns true if the Mouse keys tray is visible.
bool IsVisible() {
  return GetTray()->GetVisible();
}

}  // namespace

class MouseKeysTrayTest : public AshTestBase {
 public:
  MouseKeysTrayTest() = default;

  MouseKeysTrayTest(const MouseKeysTrayTest&) = delete;
  MouseKeysTrayTest& operator=(const MouseKeysTrayTest&) = delete;

  ~MouseKeysTrayTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMouseKeys);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);

    EXPECT_TRUE(GetImageView());
    EXPECT_TRUE(IsVisible());
  }

 private:
  // Gets the current tray image view.
  views::ImageView* GetImageView() { return GetTray()->GetIcon(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the icon disappears when mouse keys is disabled and re-appears
// when it is enabled.
TEST_F(MouseKeysTrayTest, ShowsAndHidesWithMouseKeysEnabled) {
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(false);
  EXPECT_FALSE(IsVisible());
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  EXPECT_TRUE(IsVisible());
}

// Trivial test to increase coverage of mouse_keys_tray.h. The
// MouseKeysTray does not have a bubble, so these are empty functions.
// Without this test, coverage of mouse_keys_tray.h is 0%.
TEST_F(MouseKeysTrayTest, OverriddenFunctionsDoNothing) {
  GetTray()->HideBubbleWithView(nullptr);
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  GetTray()->ClickedOutsideBubble(event);
}

using MouseKeysTrayTestFeatureDisabled = AshTestBase;

TEST_F(MouseKeysTrayTestFeatureDisabled, TrayHidden) {
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  // Tray should exist even when the feature is disabled.
  EXPECT_TRUE(GetTray());
  // However, it shouldn't be visible.
  EXPECT_FALSE(IsVisible());
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(false);
  EXPECT_FALSE(IsVisible());
}

}  // namespace ash
