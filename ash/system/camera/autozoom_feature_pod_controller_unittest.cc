// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_feature_pod_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AutozoomFeaturePodControllerTest : public AshTestBase {
 public:
  AutozoomFeaturePodControllerTest() = default;

  // AshTestBase:
  void TearDown() override {
    tile_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    controller_ = std::make_unique<AutozoomFeaturePodController>();
    tile_ = controller_->CreateTile();
  }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  void PressIcon() { controller_->OnIconPressed(); }

 private:
  std::unique_ptr<AutozoomFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(AutozoomFeaturePodControllerTest, ButtonVisibility) {
  // By default autozoom is not supported, so the button is not visible.
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());

  // If autozoom is supported, the button is visible.
  Shell::Get()->autozoom_controller()->set_autozoom_supported_for_test(true);
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());
}

TEST_F(AutozoomFeaturePodControllerTest, PressIconTogglesFeature) {
  CreateButton();
  ASSERT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);

  // Pressing the icon enables autozoom.
  PressIcon();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::ON_SINGLE);

  // Pressing the icon again disables autozoom.
  PressIcon();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);
}

}  // namespace ash
