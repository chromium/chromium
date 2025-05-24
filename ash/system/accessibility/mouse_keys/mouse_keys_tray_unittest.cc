// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

MouseKeysTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->mouse_keys_tray();
}

// Returns true if the Mouse keys tray is visible.
bool IsVisible() {
  return GetTray()->GetVisible();
}

bool IsActive() {
  return GetTray()->is_active();
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

  // Gets the current tray image view.
  views::ImageView* GetImageView() { return GetTray()->GetIcon(); }

 private:
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

// Tests that the accessible name is set correctly in the accessibility cache.
TEST_F(MouseKeysTrayTest, AccessibleName) {
  ui::AXNodeData tray_data;
  GetTray()->GetViewAccessibility().GetAccessibleNodeData(&tray_data);
  EXPECT_EQ(tray_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE));
}

// Tests that the mouse keys icon is activated and deactivated when unpaused
// and paused respectively.
TEST_F(MouseKeysTrayTest, MouseKeysTrayStateChangesOnPause) {
  // When mouse keys is initially enabled, it should also be unpaused.
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  EXPECT_FALSE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE(IsActive());

  ui::AXNodeData tray_data;
  GetTray()->GetViewAccessibility().GetAccessibleNodeData(&tray_data);

  Shell::Get()->accessibility_controller()->ToggleMouseKeys();
  EXPECT_TRUE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_EQ(GetImageView()->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_RESUME));
  EXPECT_EQ(tray_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE));
  EXPECT_FALSE(IsActive());

  GetTray()->GetViewAccessibility().GetAccessibleNodeData(&tray_data);
  Shell::Get()->accessibility_controller()->ToggleMouseKeys();
  EXPECT_FALSE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_TRUE(IsActive());
  EXPECT_EQ(GetImageView()->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE));
  EXPECT_EQ(tray_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_RESUME));
}

// Tests that the mouse keys icon stays active when enabled from a previously
// paused state
TEST_F(MouseKeysTrayTest, MouseKeysActiveOnReenable) {
  // When mouse keys is initially enabled, it should also be unpaused.
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  EXPECT_FALSE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE(IsActive());

  // Pause mouse keys
  Shell::Get()->accessibility_controller()->ToggleMouseKeys();
  EXPECT_TRUE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_FALSE(IsActive());

  // Disable mouse keys
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(false);
  EXPECT_FALSE(IsVisible());

  // Re-enable mouse keys, though paused before, it should be unpaused now.
  Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  EXPECT_FALSE(Shell::Get()->mouse_keys_controller()->paused());
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE(IsActive());
}

}  // namespace ash
