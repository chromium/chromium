// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"

#include <memory>

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CaptureModeFeaturePodControllerTest : public AshTestBase {
 public:
  CaptureModeFeaturePodControllerTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    tile_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    auto* tray_controller = GetPrimaryUnifiedSystemTray()
                                ->bubble()
                                ->unified_system_tray_controller();
    controller_ =
        std::make_unique<CaptureModeFeaturePodController>(tray_controller);
      tile_ = controller_->CreateTile();
  }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  void PressIcon() { controller_->OnIconPressed(); }

 private:
  std::unique_ptr<CaptureModeFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(CaptureModeFeaturePodControllerTest, ButtonVisibility) {
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

TEST_F(CaptureModeFeaturePodControllerTest, PressIconStartsCaptureMode) {
  CreateButton();
  ASSERT_FALSE(capture_mode_util::IsCaptureModeActive());

  PressIcon();
  EXPECT_TRUE(capture_mode_util::IsCaptureModeActive());
}

}  // namespace ash
