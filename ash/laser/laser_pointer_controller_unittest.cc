// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller.h"

#include "ash/laser/laser_pointer_controller_test_api.h"
#include "ash/laser/laser_pointer_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class LaserPointerControllerTest : public AshTestBase {
 public:
  LaserPointerControllerTest() = default;
  ~LaserPointerControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<LaserPointerController>();
    controller_test_api_ =
        std::make_unique<LaserPointerControllerTestApi>(controller_.get());
  }

  void TearDown() override {
    controller_test_api_.reset();
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<LaserPointerController> controller_;
  std::unique_ptr<LaserPointerControllerTestApi> controller_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaserPointerControllerTest);
};

}  // namespace

// Test to ensure the class responsible for drawing the laser pointer receives
// points from stylus movements as expected.
TEST_F(LaserPointerControllerTest, LaserPointerRenderer) {
  // The laser pointer mode only works with stylus.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  // When disabled the laser pointer should not be showing.
  event_generator->MoveTouch(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

  // Verify that by enabling the mode, the laser pointer should still not be
  // showing.
  controller_test_api_->SetEnabled(true);
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

  // Verify moving the stylus 4 times will not display the laser pointer.
  event_generator->MoveTouch(gfx::Point(2, 2));
  event_generator->MoveTouch(gfx::Point(3, 3));
  event_generator->MoveTouch(gfx::Point(4, 4));
  event_generator->MoveTouch(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

  // Verify pressing the stylus will show the laser pointer and add a point but
  // will not activate fading out.
  event_generator->PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());
  EXPECT_EQ(1, controller_test_api_->laser_points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  event_generator->MoveTouch(gfx::Point(6, 6));
  event_generator->MoveTouch(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_->laser_points().GetNumberOfPoints());

  // Verify releasing the stylus still shows the laser pointer, which is fading
  // away.
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode does not display the laser pointer.
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  EXPECT_FALSE(controller_test_api_->IsFadingAway());

  // Verify that disabling the mode while laser pointer is displayed does not
  // display the laser pointer.
  controller_test_api_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

  // Verify that the laser pointer does not add points while disabled.
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(8, 8));
  event_generator->ReleaseTouch();
  event_generator->MoveTouch(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

  // Verify that the laser pointer does not get shown if points are not coming
  // from the stylus, even when enabled.
  event_generator->ExitPenPointerMode();
  controller_test_api_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  event_generator->ReleaseTouch();
}

// Test to ensure the class responsible for drawing the laser pointer handles
// prediction as expected when it receives points from stylus movements.
TEST_F(LaserPointerControllerTest, LaserPointerPrediction) {
  controller_test_api_->SetEnabled(true);
  // The laser pointer mode only works with stylus.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());

  EXPECT_EQ(1, controller_test_api_->laser_points().GetNumberOfPoints());
  // Initial press event shouldn't generate any predicted points as there's no
  // history to use for prediction.
  EXPECT_EQ(0,
            controller_test_api_->predicted_laser_points().GetNumberOfPoints());

  // Verify dragging the stylus 3 times will add some predicted points.
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(20, 20));
  event_generator->MoveTouch(gfx::Point(30, 30));
  EXPECT_NE(0,
            controller_test_api_->predicted_laser_points().GetNumberOfPoints());
  // Verify predicted points are in the right direction.
  for (const auto& point :
       controller_test_api_->predicted_laser_points().points()) {
    EXPECT_LT(30, point.location.x());
    EXPECT_LT(30, point.location.y());
  }

  // Verify releasing the stylus removes predicted points.
  event_generator->ReleaseTouch();
  EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
  EXPECT_TRUE(controller_test_api_->IsFadingAway());
  EXPECT_EQ(0,
            controller_test_api_->predicted_laser_points().GetNumberOfPoints());
}

}  // namespace ash
