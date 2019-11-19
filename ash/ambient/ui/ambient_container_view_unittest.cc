// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class TestPhotoController : public PhotoController {
 public:
  TestPhotoController() = default;
  ~TestPhotoController() override = default;

  // PhotoController:
  void GetNextImage(PhotoController::PhotoDownloadCallback callback) override {
    gfx::ImageSkia image =
        gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
    std::move(callback).Run(image);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPhotoController);
};

}  // namespace

class AmbientContainerViewTest : public AshTestBase {
 public:
  AmbientContainerViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AmbientContainerViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kAmbientModeFeature);
    photo_controller_ = std::make_unique<TestPhotoController>();
    AshTestBase::SetUp();
  }

  void Toggle() { AmbientController()->Toggle(); }

  AmbientContainerView* GetView() {
    return AmbientController()->get_container_view_for_testing();
  }

  const base::OneShotTimer& GetTimer() const {
    return AmbientController()->get_timer_for_testing();
  }

 private:
  AmbientController* AmbientController() const {
    return Shell::Get()->ambient_controller();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestPhotoController> photo_controller_;

  DISALLOW_COPY_AND_ASSIGN(AmbientContainerViewTest);
};

// Shows the widget for AmbientContainerView.
TEST_F(AmbientContainerViewTest, Show) {
  // Show the widget.
  Toggle();
  AmbientContainerView* ambient_container_view = GetView();
  EXPECT_TRUE(ambient_container_view);

  views::Widget* widget = ambient_container_view->GetWidget();
  EXPECT_TRUE(widget);
}

// Tests that the window can be shown or closed by toggling.
TEST_F(AmbientContainerViewTest, ToggleWindow) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Call |Toggle()| to close the widget.
  Toggle();
  EXPECT_FALSE(GetView());
}

// Tests that AmbientContainerView window should be fullscreen.
TEST_F(AmbientContainerViewTest, WindowFullscreenSize) {
  // Show the widget.
  Toggle();
  views::Widget* widget = GetView()->GetWidget();

  gfx::Rect root_window_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow()->GetRootWindow())
          .bounds();
  gfx::Rect container_window_bounds =
      widget->GetNativeWindow()->GetBoundsInScreen();
  EXPECT_EQ(root_window_bounds, container_window_bounds);
}

// Tests that the timer is running on showing and stopped on closing.
TEST_F(AmbientContainerViewTest, TimerRunningWhenShowing) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Download |kImageBufferLength| / 2 + 1 images to fill buffer in PhotoModel,
  // in order to return false in |ShouldFetchImmediately()| and start timer.
  const int num_image_to_load = kImageBufferLength / 2 + 1;
  task_environment_->FastForwardBy(kAnimationDuration * num_image_to_load);

  EXPECT_TRUE(GetTimer().IsRunning());

  // Call |Toggle()| to close the widget.
  Toggle();
  EXPECT_FALSE(GetView());
  EXPECT_FALSE(GetTimer().IsRunning());
}

// Tests that a mouse click closes the widget and stops the timer.
TEST_F(AmbientContainerViewTest, MouseClickClosesWidgetAndStopsTimer) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Download |kImageBufferLength| / 2 + 1 images to fill buffer in PhotoModel,
  // in order to return false in |ShouldFetchImmediately()| and start timer.
  const int num_image_to_load = kImageBufferLength / 2 + 1;
  task_environment_->FastForwardBy(kAnimationDuration * num_image_to_load);
  EXPECT_TRUE(GetTimer().IsRunning());

  // Simulate mouse click to close the widget.
  ui::test::EventGenerator generator(
      GetView()->GetWidget()->GetNativeWindow()->GetRootWindow());
  generator.PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetView());
  EXPECT_FALSE(GetTimer().IsRunning());
}

}  // namespace ash
