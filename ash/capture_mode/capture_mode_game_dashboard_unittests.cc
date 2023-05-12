// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class GameDashboardCaptureModeTest : public AshTestBase {
 public:
  GameDashboardCaptureModeTest()
      : scoped_feature_list_(features::kGameDashboard) {}
  GameDashboardCaptureModeTest(const GameDashboardCaptureModeTest&) = delete;
  GameDashboardCaptureModeTest& operator=(const GameDashboardCaptureModeTest&) =
      delete;
  ~GameDashboardCaptureModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_RELEASE_TRACK=testimage-channel",
        base::SysInfo::GetLsbReleaseTime());
    AshTestBase::SetUp();
    EXPECT_TRUE(features::IsGameDashboardEnabled());

    window_ = CreateTestWindow();
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
    base::SysInfo::ResetChromeOSVersionInfoForTest();
  }

  CaptureModeController* StartGameCaptureModeSession() {
    auto* controller = CaptureModeController::Get();
    controller->Start(CaptureModeEntryType::kGameDashboard);
    CHECK(controller->IsActive());
    return controller;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<aura::Window> window_;
};

TEST_F(GameDashboardCaptureModeTest, GameDashboardBehavior) {
  CaptureModeController* controller = StartGameCaptureModeSession();
  CaptureModeSession* session = controller->capture_mode_session();
  CaptureModeBehavior* active_behavior = session->active_behavior();
  ASSERT_TRUE(active_behavior);

  EXPECT_FALSE(active_behavior->ShouldImageCaptureTypeBeAllowed());
  EXPECT_TRUE(active_behavior->ShouldVideoCaptureTypeBeAllowed());
  EXPECT_FALSE(active_behavior->ShouldFulscreenCaptureSourceBeAllowed());
  EXPECT_FALSE(active_behavior->ShouldRegionCaptureSourceBeAllowed());
  EXPECT_TRUE(active_behavior->ShouldWindowCaptureSourceBeAllowed());
  EXPECT_TRUE(
      active_behavior->SupportsAudioRecordingMode(AudioRecordingMode::kOff));
  EXPECT_TRUE(active_behavior->SupportsAudioRecordingMode(
      features::IsCaptureModeAudioMixingEnabled()
          ? AudioRecordingMode::kSystemAndMicrophone
          : AudioRecordingMode::kMicrophone));
  EXPECT_TRUE(active_behavior->ShouldCameraSelectionSettingsBeIncluded());
  EXPECT_FALSE(active_behavior->ShouldDemoToolsSettingsBeIncluded());
  EXPECT_TRUE(active_behavior->ShouldSaveToSettingsBeIncluded());
  EXPECT_FALSE(active_behavior->ShouldGifBeSupported());
  EXPECT_TRUE(active_behavior->ShouldShowPreviewNotification());
  EXPECT_FALSE(active_behavior->ShouldSkipVideoRecordingCountDown());
  EXPECT_FALSE(active_behavior->ShouldCreateRecordingOverlayController());
  EXPECT_FALSE(active_behavior->ShouldShowUserNudge());
  EXPECT_TRUE(active_behavior->ShouldAutoSelectFirstCamera());
}

TEST_F(GameDashboardCaptureModeTest, CaptureBar) {
  StartGameCaptureModeSession();
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  // Checks that the game capture bar only includes the start recording button,
  // settings button and close button.
  EXPECT_TRUE(GetStartRecordingButton());
  EXPECT_FALSE(GetImageToggleButton());
  EXPECT_FALSE(GetVideoToggleButton());
  EXPECT_FALSE(GetFullscreenToggleButton());
  EXPECT_FALSE(GetRegionToggleButton());
  EXPECT_FALSE(GetWindowToggleButton());
  EXPECT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetCloseButton());
}

}  // namespace ash
