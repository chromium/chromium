// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/laser/laser_pointer_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/fast_ink/laser/laser_pointer_controller_test_api.h"
#include "ash/fast_ink/laser/laser_pointer_view.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class TestLaserPointerObserver : public LaserPointerObserver {
 public:
  TestLaserPointerObserver() = default;
  ~TestLaserPointerObserver() override = default;

  // LaserPointerObserver:
  void OnLaserPointerStateChanged(bool enabled) override {
    laser_pointer_enabled_ = enabled;
  }

  bool laser_pointer_enabled() { return laser_pointer_enabled_; }

 private:
  bool laser_pointer_enabled_ = false;
};

class LaserPointerControllerTest : public AshTestBase {
 public:
  LaserPointerControllerTest() = default;
  LaserPointerControllerTest(const LaserPointerControllerTest&) = delete;
  LaserPointerControllerTest& operator=(const LaserPointerControllerTest&) =
      delete;
  ~LaserPointerControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<TestLaserPointerObserver>();
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
  void VerifyLaserPointerRendererTouchEvent() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();

    // When disabled the laser pointer should not be showing.
    event_generator->MoveTouch(gfx::Point(1, 1));
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify that by enabling the mode, the laser pointer should still not be
    // showing.
    controller_test_api_->SetEnabled(true);
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify moving the finger 4 times will not display the laser pointer.
    event_generator->MoveTouch(gfx::Point(2, 2));
    event_generator->MoveTouch(gfx::Point(3, 3));
    event_generator->MoveTouch(gfx::Point(4, 4));
    event_generator->MoveTouch(gfx::Point(5, 5));
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify pressing the finger will show the laser pointer and add a point
    // but will not activate fading out.
    event_generator->PressTouch();
    EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
    EXPECT_FALSE(controller_test_api_->IsFadingAway());
    EXPECT_EQ(1, controller_test_api_->laser_points().GetNumberOfPoints());

    // Verify dragging the finger 2 times will add 2 more points.
    event_generator->MoveTouch(gfx::Point(6, 6));
    event_generator->MoveTouch(gfx::Point(7, 7));
    EXPECT_EQ(3, controller_test_api_->laser_points().GetNumberOfPoints());

    // Verify releasing the finger still shows the laser pointer, which is
    // fading away.
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
  }

  void VerifyLaserPointerRendererMouseEvent() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();

    // When disabled the laser pointer should not be showing.
    event_generator->MoveMouseTo(gfx::Point(1, 1));
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify that by enabling the mode, the laser pointer should still not be
    // showing.
    controller_test_api_->SetEnabled(true);
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify moving the cursor 4 times will display the laser pointer.
    event_generator->MoveMouseTo(gfx::Point(2, 2));
    event_generator->MoveMouseTo(gfx::Point(3, 3));
    event_generator->MoveMouseTo(gfx::Point(4, 4));
    event_generator->MoveMouseTo(gfx::Point(5, 5));
    EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
    EXPECT_FALSE(controller_test_api_->IsFadingAway());
    EXPECT_EQ(4, controller_test_api_->laser_points().GetNumberOfPoints());

    // Verify moving the cursor 2 times will add 2 more points.
    event_generator->MoveMouseTo(gfx::Point(6, 6));
    event_generator->MoveMouseTo(gfx::Point(7, 7));
    EXPECT_EQ(6, controller_test_api_->laser_points().GetNumberOfPoints());

    // Verify that disabling the mode does not display the laser pointer.
    controller_test_api_->SetEnabled(false);
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
    EXPECT_FALSE(controller_test_api_->IsFadingAway());

    // Verify that disabling the mode while laser pointer is displayed does not
    // display the laser pointer.
    controller_test_api_->SetEnabled(true);
    event_generator->MoveMouseTo(gfx::Point(6, 6));
    EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer());
    controller_test_api_->SetEnabled(false);
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());

    // Verify that the laser pointer does not add points while disabled.
    event_generator->MoveMouseTo(gfx::Point(8, 8));
    event_generator->MoveMouseTo(gfx::Point(9, 9));
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  }

  TestLaserPointerObserver* observer() { return observer_.get(); }

  std::unique_ptr<LaserPointerController> controller_;
  std::unique_ptr<LaserPointerControllerTestApi> controller_test_api_;
  std::unique_ptr<TestLaserPointerObserver> observer_;
};

}  // namespace

// Test to ensure the class responsible for drawing the laser pointer receives
// points from stylus movements as expected.
TEST_F(LaserPointerControllerTest, LaserPointerRenderer) {
  stylus_utils::SetHasStylusInputForTesting();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  VerifyLaserPointerRendererTouchEvent();

  // Verify that the laser pointer does not get shown if points are not coming
  // from the stylus, even when enabled.
  event_generator->ExitPenPointerMode();
  controller_test_api_->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(10, 10));
  event_generator->MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  event_generator->ReleaseTouch();

  // Make sure that event can be sent after the pointer widget is destroyed
  // by release. This can happen if the pen event causes the deletion of
  // the pointer event in an earlier event handler.
  ui::PointerDetails pointer_details;
  pointer_details.pointer_type = ui::EventPointerType::kPen;

  ui::TouchEvent touch(ui::EventType::kTouchMoved, gfx::PointF(), gfx::PointF(),
                       base::TimeTicks(), pointer_details, 0);
  ui::Event::DispatcherApi api(&touch);
  api.set_target(Shell::GetPrimaryRootWindow());
  static_cast<ui::EventHandler*>(controller_.get())->OnTouchEvent(&touch);
}

// Test to ensure the class responsible for drawing the laser pointer receives
// points from mouse movements as expected.
TEST_F(LaserPointerControllerTest, LaserPointerRendererTouchEvent) {
  stylus_utils::SetNoStylusInputForTesting();
  VerifyLaserPointerRendererTouchEvent();

  // Make sure that event can be sent after the pointer widget is destroyed
  // by release. This can happen if the touch event causes the deletion of
  // the pointer event in an earlier event handler.
  ui::PointerDetails pointer_details;

  ui::TouchEvent touch(ui::EventType::kTouchMoved, gfx::PointF(), gfx::PointF(),
                       base::TimeTicks(), pointer_details, 0);
  ui::Event::DispatcherApi api(&touch);
  api.set_target(Shell::GetPrimaryRootWindow());
  static_cast<ui::EventHandler*>(controller_.get())->OnTouchEvent(&touch);
}

// Test to ensure the class responsible for drawing the laser pointer receives
// points from mouse movements as expected when stylus input is not available.
TEST_F(LaserPointerControllerTest, LaserPointerRendererMouseEventNoStylus) {
  stylus_utils::SetNoStylusInputForTesting();

  VerifyLaserPointerRendererMouseEvent();
}

// Test to ensure the class responsible for drawing the laser pointer receives
// points from mouse movements as expected when stylus input is available but
// hasn't been seen before.
TEST_F(LaserPointerControllerTest, LaserPointerRendererMouseEventHasStylus) {
  stylus_utils::SetHasStylusInputForTesting();

  VerifyLaserPointerRendererMouseEvent();

  // Verify that the laser pointer does not get shown if points are coming from
  // mouse event if a stylus interaction has been seen.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch();
  event_generator->MoveMouseTo(gfx::Point(2, 2));
  event_generator->MoveMouseTo(gfx::Point(3, 3));
  event_generator->MoveMouseTo(gfx::Point(4, 4));
  event_generator->MoveMouseTo(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
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

TEST_F(LaserPointerControllerTest, NotifyLaserPointerStateChanged) {
  controller_->AddObserver(observer());
  controller_test_api_->SetEnabled(true);
  EXPECT_TRUE(observer()->laser_pointer_enabled());
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(observer()->laser_pointer_enabled());
  controller_->RemoveObserver(observer());
}

// Test to ensure the class responsible for update cursor visibility state when
// it handles mouse and touch events.
TEST_F(LaserPointerControllerTest, MouseCursorState) {
  ash::stylus_utils::SetNoStylusInputForTesting();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Verify that when disabled the cursor should be visible.
  event_generator->MoveMouseTo(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Verify that by enabling the mode, mouse cursor should be hidden.
  controller_test_api_->SetEnabled(true);
  event_generator->MoveMouseTo(gfx::Point(2, 2));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Verify that after moving with touch, mouse cursor should be still hidden
  // but unlocked.
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(2, 2));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(1, controller_test_api_->laser_points().GetNumberOfPoints());

  // Verify that moving the mouse cursor shows the cursor.
  event_generator->MoveMouseTo(gfx::Point(6, 6));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(2, controller_test_api_->laser_points().GetNumberOfPoints());

  // Verify that by disabling the mode, mouse cursor should be visible.
  controller_test_api_->SetEnabled(false);
  EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer());
  event_generator->MoveMouseTo(gfx::Point(7, 7));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
}

// Base class for tests that rely on palette.
class LaserPointerControllerTestWithPalette
    : public LaserPointerControllerTest {
 public:
  LaserPointerControllerTestWithPalette() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnablePaletteOnAllDisplays);
    stylus_utils::SetHasStylusInputForTesting();
  }
  LaserPointerControllerTestWithPalette(
      const LaserPointerControllerTestWithPalette&) = delete;
  LaserPointerControllerTestWithPalette& operator=(
      const LaserPointerControllerTestWithPalette&) = delete;
  ~LaserPointerControllerTestWithPalette() override = default;
};

// Verify that clicking a palette with the laser pointer does not
// cause the laser to show.
TEST_F(LaserPointerControllerTestWithPalette, LaserPointerPaletteDisable) {
  // Make the two displays different size to catch root coordinates
  // being used as screen coordinates.
  UpdateDisplay("800x600,1024x768");

  std::vector<display::Display> testcases{
      GetPrimaryDisplay(),
      GetSecondaryDisplay(),
  };

  for (std::size_t i = 0; i < testcases.size(); i++) {
    display::Display display = testcases[i];
    PaletteTray* palette =
        controller_test_api_->GetPaletteTrayOnDisplay(display.id());
    ASSERT_TRUE(palette) << "While processing testcase " << i;

    // The laser pointer mode only works with stylus.
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->EnterPenPointerMode();

    // Palette does not appear until a stylus is seen for the first time.
    event_generator->PressMoveAndReleaseTouchTo(display.bounds().CenterPoint());
    gfx::Point center = palette->GetBoundsInScreen().CenterPoint();

    // Tap the laser pointer both on and off of the palette.
    controller_test_api_->SetEnabled(true);
    event_generator->PressTouch(center);
    EXPECT_FALSE(controller_test_api_->IsShowingLaserPointer())
        << "While processing testcase " << i;
    event_generator->ReleaseTouch();
    event_generator->PressTouch(display.bounds().CenterPoint());
    EXPECT_TRUE(controller_test_api_->IsShowingLaserPointer())
        << "While processing testcase " << i;
    event_generator->ReleaseTouch();
  }
}

}  // namespace ash
