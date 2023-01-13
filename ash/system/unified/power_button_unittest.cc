// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/power_button.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Tests for `PowerButton`, which is inited with no user session to test the
// non-login state.
class PowerButtonTest : public NoSessionAshTestBase {
 public:
  PowerButtonTest() = default;
  PowerButtonTest(const PowerButtonTest&) = delete;
  PowerButtonTest& operator=(const PowerButtonTest&) = delete;
  ~PowerButtonTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
    NoSessionAshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    // Use a container and put the button at the bottom to give the menu enough
    // space to show, since the menu is set to be popped up to the top right of
    // the button.
    auto* container = widget_->SetContentsView(std::make_unique<views::View>());
    auto* layout =
        container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
    button_ = container->AddChildView(std::make_unique<PowerButton>());
  }

  void TearDown() override {
    widget_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  views::MenuItemView* GetMenuView() {
    return button_->GetMenuViewForTesting();
  }

  bool IsMenuShowing() { return button_->IsMenuShowing(); }

  views::View* GetRestartButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_RESTART_MENU_BUTTON);
  }

  views::View* GetPowerOffButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_OFF_MENU_BUTTON);
  }

  views::View* GetSignOutButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON);
  }

  views::View* GetLockButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_LOCK_MENU_BUTTON);
  }

  PowerButton* GetPowerButton() { return button_; }

  ui::Layer* GetBackgroundLayer() { return button_->background_view_->layer(); }

  // Simulates mouse press event on the power button. The generator click
  // does not work anymore since menu is a nested run loop.
  void SimulatePowerButtonPress() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                         button_->GetBoundsInScreen().CenterPoint(),
                         button_->GetBoundsInScreen().CenterPoint(),
                         ui::EventTimeForNow(), 0, 0);
    button_->button_content_->NotifyClick(event);
  }

 private:
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget_`.
  PowerButton* button_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
};

// `PowerButton` should be with the correct view id and have the UMA tracking
// with the correct catalog name.
TEST_F(PowerButtonTest, ButtonNameAndUMA) {
  CreateUserSessions(1);

  // No metrics logged before clicking on any buttons.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/0);

  // The power button is visible and with the corresponding id.
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_POWER_BUTTON, GetPowerButton()->GetID());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/1);
  EXPECT_TRUE(IsMenuShowing());

  // Show all buttons in the menu.
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());

  LeftClickOn(GetLockButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerLockMenuButton,
                                      /*expected_count=*/1);

  // Clicks on the power button.
  SimulatePowerButtonPress();

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/3);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/2);
  EXPECT_TRUE(IsMenuShowing());

  LeftClickOn(GetSignOutButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/4);
  histogram_tester->ExpectBucketCount(
      "Ash.QuickSettings.Button.Activated",
      QsButtonCatalogName::kPowerSignoutMenuButton,
      /*expected_count=*/1);

  // Clicks on the power button.
  SimulatePowerButtonPress();

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/5);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/3);
  EXPECT_TRUE(IsMenuShowing());

  LeftClickOn(GetRestartButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/6);
  histogram_tester->ExpectBucketCount(
      "Ash.QuickSettings.Button.Activated",
      QsButtonCatalogName::kPowerRestartMenuButton,
      /*expected_count=*/1);

  // Clicks on the power button.
  SimulatePowerButtonPress();

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/7);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/4);
  EXPECT_TRUE(IsMenuShowing());

  LeftClickOn(GetPowerOffButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/8);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerOffMenuButton,
                                      /*expected_count=*/1);
}

// No lock and sign out buttons in the menu before login.
TEST_F(PowerButtonTest, ButtonStatesNotLoggedIn) {
  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  // Only show power off and resatart buttons.
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// All buttons are shown after login.
TEST_F(PowerButtonTest, ButtonStatesLoggedIn) {
  CreateUserSessions(1);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  // Show all buttons in the menu.
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// The lock button are hidden at the lock screen.
TEST_F(PowerButtonTest, ButtonStatesLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// The lock button is hidden when adding a second multiprofile user.
TEST_F(PowerButtonTest, ButtonStatesAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// Power button's rounded radii should change correctly when switching between
// active/inactive.
TEST_F(PowerButtonTest, ButtonRoundedRadii) {
  CreateUserSessions(1);

  // Sets a LTR locale.
  base::i18n::SetICUDefaultLocale("en_US");

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_EQ(gfx::RoundedCornersF(4, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Click on a random button to close the menu.
  LeftClickOn(GetLockButton());

  // Sets a RTL locale.
  base::i18n::SetICUDefaultLocale("ar");

  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_EQ(gfx::RoundedCornersF(16, 4, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());
}

}  // namespace ash
