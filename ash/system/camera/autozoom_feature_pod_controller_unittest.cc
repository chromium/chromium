// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Tests are parameterized by feature QsRevamp.
class AutozoomFeaturePodControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AutozoomFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  // AshTestBase:
  void TearDown() override {
    tile_.reset();
    button_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    controller_ = std::make_unique<AutozoomFeaturePodController>();
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
  std::unique_ptr<AutozoomFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         AutozoomFeaturePodControllerTest,
                         testing::Bool());

TEST_P(AutozoomFeaturePodControllerTest, ButtonVisibility) {
  // By default autozoom is not supported, so the button is not visible.
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());

  // If autozoom is supported, the button is visible.
  Shell::Get()->autozoom_controller()->set_autozoom_supported_for_test(true);
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());
}

TEST_P(AutozoomFeaturePodControllerTest, PressIconTogglesFeature) {
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
