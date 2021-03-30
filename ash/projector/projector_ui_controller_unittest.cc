// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/marker/marker_controller.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ProjectorUiControllerTest : public AshTestBase {
 public:
  ProjectorUiControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kProjector);
  }

  ProjectorUiControllerTest(const ProjectorUiControllerTest&) = delete;
  ProjectorUiControllerTest& operator=(const ProjectorUiControllerTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ =
        static_cast<ProjectorControllerImpl*>(ProjectorController::Get())
            ->ui_controller();
  }

 protected:
  ProjectorUiController* controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that toggling on the laser pointer on Projector tools propagates to
// the laser pointer controller and marker controller.
TEST_F(ProjectorUiControllerTest, EnablingDisablingLaserPointer) {
  auto* laser_pointer_controller_ = Shell::Get()->laser_pointer_controller();
  auto* marker_controller_ = MarkerController::Get();
  controller_->ShowToolbar();

  // Reset enable states.
  laser_pointer_controller_->SetEnabled(false);
  marker_controller_->SetEnabled(false);

  // Toggling laser pointer on.
  controller_->OnLaserPointerPressed();
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());

  // Toggling laser pointer off.
  controller_->OnLaserPointerPressed();
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());

  // Verify that toggling laser pointer disables marker when it was enabled.
  marker_controller_->SetEnabled(true);
  EXPECT_TRUE(marker_controller_->is_enabled());
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
  controller_->OnLaserPointerPressed();
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());
}

// Verifies that toggling on the marker on Projector tools propagates to
// the laser pointer controller and marker controller.
TEST_F(ProjectorUiControllerTest, EnablingDisablingMarker) {
  auto* laser_pointer_controller_ = Shell::Get()->laser_pointer_controller();
  auto* marker_controller_ = MarkerController::Get();
  controller_->ShowToolbar();

  // Reset enable states.
  laser_pointer_controller_->SetEnabled(false);
  marker_controller_->SetEnabled(false);

  // Toggling marker on.
  controller_->OnMarkerPressed();
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
  EXPECT_TRUE(marker_controller_->is_enabled());

  // Toggling marker off.
  controller_->OnMarkerPressed();
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
  EXPECT_FALSE(marker_controller_->is_enabled());

  // Verify that toggling marker disables laser pointer when it was enabled.
  laser_pointer_controller_->SetEnabled(true);
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());
  controller_->OnMarkerPressed();
  EXPECT_TRUE(marker_controller_->is_enabled());
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());

  // Laser pointer has more than one entry points.
  // Verify that marker will be disabled even if laser pointer is enabled by
  // others.
  laser_pointer_controller_->SetEnabled(true);
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());
  // Verify that marker will not be enabled if laser pointer is disabled by
  // others.
  laser_pointer_controller_->SetEnabled(false);
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
}

}  // namespace ash