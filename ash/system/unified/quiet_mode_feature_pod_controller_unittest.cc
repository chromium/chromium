// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quiet_mode_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

// Tests manually control their session state.
class QuietModeFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  QuietModeFeaturePodControllerTest() = default;

  QuietModeFeaturePodControllerTest(const QuietModeFeaturePodControllerTest&) =
      delete;
  QuietModeFeaturePodControllerTest& operator=(
      const QuietModeFeaturePodControllerTest&) = delete;

  ~QuietModeFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    TearDownButton();
    NoSessionAshTestBase::TearDown();
  }

  void SetUpButton() {
    auto* system_tray = GetPrimaryUnifiedSystemTray();
    if (!system_tray->IsBubbleShown()) {
      system_tray->ShowBubble();
    }
    controller_ = std::make_unique<QuietModeFeaturePodController>();
    tile_ = controller_->CreateTile();
  }

  void TearDownButton() {
    tile_.reset();
    controller_.reset();
  }

  UnifiedSystemTrayController* tray_controller() {
    DCHECK(GetPrimaryUnifiedSystemTray()->bubble());
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  bool IsButtonToggled() { return tile_->IsToggled(); }

 private:
  std::unique_ptr<QuietModeFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(QuietModeFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_F(QuietModeFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it should be visible.
  EXPECT_TRUE(IsButtonVisible());
}

TEST_F(QuietModeFeaturePodControllerTest, ButtonVisibilityLocked) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  SetUpButton();
  // If locked, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_F(QuietModeFeaturePodControllerTest, IconUMATracking) {
  CreateUserSessions(1);
  SetUpButton();
  message_center::MessageCenter::Get()->SetQuietMode(false);

  std::string histogram_prefix;
    histogram_prefix = "Ash.QuickSettings.FeaturePod.";

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "DiveIn",
                                     /*expected_count=*/0);

  // Turn on quiet mode when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(histogram_prefix + "ToggledOn",
                                      QsFeatureCatalogName::kQuietMode,
                                      /*expected_count=*/1);

  // Turn off quiet mode when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOff",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(histogram_prefix + "DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(histogram_prefix + "ToggledOff",
                                      QsFeatureCatalogName::kQuietMode,
                                      /*expected_count=*/1);
}

TEST_F(QuietModeFeaturePodControllerTest, ToggledState) {
  CreateUserSessions(1);

  // Do not disturb is initially off, button is not toggled.
  SetUpButton();
  EXPECT_FALSE(message_center::MessageCenter::Get()->IsQuietMode());
  EXPECT_FALSE(IsButtonToggled());

  // Button is toggled when QuietMode is on.
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(message_center::MessageCenter::Get()->IsQuietMode());
  EXPECT_TRUE(IsButtonToggled());

  // Button persists state after being destroyed and recreated, such as when
  // closing and opening the QS bubble.
  TearDownButton();
  SetUpButton();
  EXPECT_TRUE(message_center::MessageCenter::Get()->IsQuietMode());
  EXPECT_TRUE(IsButtonToggled());
}

}  // namespace ash
