// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

class PrivacyScreenToastControllerTest : public AshTestBase {
 public:
  PrivacyScreenToastControllerTest() = default;

  PrivacyScreenToastControllerTest(const PrivacyScreenToastControllerTest&) =
      delete;
  PrivacyScreenToastControllerTest& operator=(
      const PrivacyScreenToastControllerTest&) = delete;

  void UpdateToastView() { toast_controller()->UpdateToastView(); }

  // End-to-end mocking of enabling the privacy screen is complex. For the
  // purposes of these unit tests, we will enable more simply.
  void UpdateEnabledForTest(bool enabled) {
    privacy_screen_controller()->current_status_ = enabled;
  }

  PrivacyScreenToastController* toast_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->privacy_screen_toast_controller_.get();
  }

  PrivacyScreenController* privacy_screen_controller() {
    return Shell::Get()->privacy_screen_controller();
  }

  TrayBubbleView* bubble_view() {
    return toast_controller()->bubble_view_.get();
  }
};

TEST_F(PrivacyScreenToastControllerTest, BubbleViewAccessibleName) {
  toast_controller()->ShowToast();

  // Test the name when enabled and managed are false.
  {
    ui::AXNodeData node_data;
    bubble_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOAST_ACCESSIBILITY_TEXT,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE),
                  std::u16string()));
  }

  UpdateEnabledForTest(true);
  EXPECT_TRUE(privacy_screen_controller()->GetEnabled());
  UpdateToastView();

  // Test the name when enabled is true and managed is false.
  {
    ui::AXNodeData node_data;
    bubble_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOAST_ACCESSIBILITY_TEXT,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE),
                  std::u16string()));
  }

  privacy_screen_controller()->SetEnforced(true);
  EXPECT_TRUE(privacy_screen_controller()->IsManaged());
  UpdateToastView();

  // Test the name when enabled and managed are true.
  {
    ui::AXNodeData node_data;
    bubble_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOAST_ACCESSIBILITY_TEXT,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE),
                  l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ENTERPRISE_MANAGED)));
  }
}

}  // namespace ash
