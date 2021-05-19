// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/marker/marker_controller.h"
#include "ash/marker/marker_controller_test_api.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_bar_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

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

  // Verify that toggling laser pointer disables magnifier when it was enabled.
  auto* magnification_controller =
      Shell::Get()->partial_magnification_controller();
  controller_->OnMagnifierButtonPressed(true);
  EXPECT_TRUE(magnification_controller->is_enabled());
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
  controller_->OnLaserPointerPressed();
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());
  EXPECT_FALSE(magnification_controller->is_enabled());
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

  // Verify that toggling marker disables magnifier when it was enabled.
  auto* magnification_controller =
      Shell::Get()->partial_magnification_controller();
  controller_->OnMagnifierButtonPressed(true);
  EXPECT_TRUE(magnification_controller->is_enabled());
  controller_->OnMarkerPressed();
  EXPECT_TRUE(marker_controller_->is_enabled());
  EXPECT_FALSE(magnification_controller->is_enabled());
}

// Verifies that clicking the Clear All Markers button and disabling marker mode
// clears all markers.
TEST_F(ProjectorUiControllerTest, ClearAllMarkers) {
  auto* marker_controller_ = MarkerController::Get();
  auto marker_controller_test_api_ =
      std::make_unique<MarkerControllerTestApi>(marker_controller_);
  controller_->ShowToolbar();

  // Reset enable states.
  marker_controller_->SetEnabled(false);

  // Toggling marker on.
  controller_->OnMarkerPressed();
  EXPECT_TRUE(marker_controller_->is_enabled());

  // No markers at the beginning.
  EXPECT_FALSE(marker_controller_test_api_->IsShowingMarker());

  // Draw something.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
  EXPECT_TRUE(marker_controller_test_api_->IsShowingMarker());

  // Clear everything.
  controller_->OnClearAllMarkersPressed();
  EXPECT_FALSE(marker_controller_test_api_->IsShowingMarker());

  // Draw some more stuff.
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
  EXPECT_TRUE(marker_controller_test_api_->IsShowingMarker());

  // Toggling marker off. This clears all markers.
  controller_->OnMarkerPressed();
  EXPECT_FALSE(marker_controller_->is_enabled());
  EXPECT_FALSE(marker_controller_test_api_->IsShowingMarker());
}

// Verifies that the bar view buttons are enabled/disabled with recording state.
TEST_F(ProjectorUiControllerTest, RecordingState) {
  controller_->ShowToolbar();
  ProjectorBarView* bar_view_ = controller_->projector_bar_view();
  EXPECT_FALSE(bar_view_->IsKeyIdeaButtonEnabled());

  controller_->OnRecordingStateChanged(/* started = */ true);
  EXPECT_TRUE(bar_view_->IsKeyIdeaButtonEnabled());
  EXPECT_TRUE(bar_view_->IsClosedCaptionEnabled());

  controller_->OnRecordingStateChanged(/* started = */ false);
  EXPECT_FALSE(bar_view_->IsKeyIdeaButtonEnabled());
  EXPECT_FALSE(bar_view_->IsClosedCaptionEnabled());
}

TEST_F(ProjectorUiControllerTest, CaptionBubbleVisible) {
  controller_->ShowToolbar();
  controller_->SetCaptionBubbleState(true);
  EXPECT_TRUE(controller_->IsCaptionBubbleModelOpen());

  controller_->SetCaptionBubbleState(false);
  EXPECT_FALSE(controller_->IsCaptionBubbleModelOpen());
}

TEST_F(ProjectorUiControllerTest, EnablingDisablingMagnifierGlass) {
  // Ensure that enabling magnifier disables marker if it was enabled.
  controller_->ShowToolbar();
  auto* marker_controller_ = MarkerController::Get();
  marker_controller_->SetEnabled(true);
  EXPECT_TRUE(marker_controller_->is_enabled());
  controller_->OnMagnifierButtonPressed(true);
  EXPECT_FALSE(marker_controller_->is_enabled());

  // Ensures that enabling magnifier disables laser pointer if it was enabled.
  auto* laser_pointer_controller_ = Shell::Get()->laser_pointer_controller();
  laser_pointer_controller_->SetEnabled(true);
  EXPECT_TRUE(laser_pointer_controller_->is_enabled());
  controller_->OnMagnifierButtonPressed(true);
  EXPECT_FALSE(laser_pointer_controller_->is_enabled());
}

}  // namespace ash
