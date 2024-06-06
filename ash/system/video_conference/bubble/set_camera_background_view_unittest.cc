// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_camera_background_view.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_utils.h"

namespace ash::video_conference {

class SetCameraBackgroundViewTest : public AshTestBase {
 public:
  SetCameraBackgroundViewTest() = default;
  SetCameraBackgroundViewTest(const SetCameraBackgroundViewTest&) = delete;
  SetCameraBackgroundViewTest& operator=(const SetCameraBackgroundViewTest&) =
      delete;
  ~SetCameraBackgroundViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    // Required for constructing SetCameraBackgroundView.
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();

    // Mock CameraEffectsController to avoid crush.
    MockCameraEffectsController();

    // This widget is required to implicitly create ColorProvider.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->GetContentsView()->AddChildView(
        std::make_unique<SetCameraBackgroundView>(nullptr, controller()));
  }

  void TearDown() override {
    AshTestBase::TearDown();
    widget_.reset();
    controller_.reset();
  }

  void MockCameraEffectsController() {
    // Enable test mode to mock the SetCameraEffects calls.
    camera_effects_controller()->bypass_set_camera_effects_for_testing(true);

    // Set fake path to bypass loading background images.
    ASSERT_TRUE(file_tmp_dir_.CreateUniqueTempDir());
    camera_effects_controller()->set_camera_background_img_dir_for_testing(
        file_tmp_dir_.GetPath());
    camera_effects_controller()->set_camera_background_run_dir_for_testing(
        file_tmp_dir_.GetPath());
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  CameraEffectsController* camera_effects_controller() {
    return Shell::Get()->camera_effects_controller();
  }

  views::View* buble_view() { return widget_->GetContentsView(); }

  SetCameraBackgroundView* set_camera_background_view() {
    return views::AsViewClass<SetCameraBackgroundView>(
        buble_view()->children().back());
  }

 private:
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;
  base::ScopedTempDir file_tmp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SetCameraBackgroundViewTest, AnimationPlayingAndStopping) {
  // Top level view should not trigger animation.
  buble_view()->SetVisible(true);
  EXPECT_FALSE(set_camera_background_view()
                   ->IsAnimationPlayingForCreateWithAiButtonForTesting());

  // set_camera_background_view_ should trigger animation.
  set_camera_background_view()->SetVisible(true);
  EXPECT_TRUE(set_camera_background_view()
                  ->IsAnimationPlayingForCreateWithAiButtonForTesting());

  // set_camera_background_view_ should stop animation.
  set_camera_background_view()->SetVisible(false);
  EXPECT_FALSE(set_camera_background_view()
                   ->IsAnimationPlayingForCreateWithAiButtonForTesting());
}

}  // namespace ash::video_conference
