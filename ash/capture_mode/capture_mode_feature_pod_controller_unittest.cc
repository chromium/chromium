// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"

#include <memory>

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Tests are parameterized by feature QsRevamp.
class CaptureModeFeaturePodControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  CaptureModeFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    tile_.reset();
    button_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    auto* tray_controller = GetPrimaryUnifiedSystemTray()
                                ->bubble()
                                ->unified_system_tray_controller();
    controller_ =
        std::make_unique<CaptureModeFeaturePodController>(tray_controller);
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_ = base::WrapUnique(controller_->CreateButton());
    }
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  void PressIcon() { controller_->OnIconPressed(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<CaptureModeFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         CaptureModeFeaturePodControllerTest,
                         testing::Bool());

TEST_P(CaptureModeFeaturePodControllerTest, ButtonVisibility) {
  // The button is visible in an active session.
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());

  // The button is not visible at the lock screen.
  GetSessionControllerClient()->LockScreen();

  // Locking the screen closes the system tray bubble, so re-show it before
  // creating the button again.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(CaptureModeFeaturePodControllerTest, PressIconStartsCaptureMode) {
  CreateButton();
  ASSERT_FALSE(capture_mode_util::IsCaptureModeActive());

  PressIcon();
  EXPECT_TRUE(capture_mode_util::IsCaptureModeActive());
}

}  // namespace ash
