// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/pill_button.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"

namespace ash {

class GameDashboardCaptureModeTest : public AshTestBase {
 public:
  GameDashboardCaptureModeTest()
      : scoped_feature_list_(features::kGameDashboard) {}
  GameDashboardCaptureModeTest(const GameDashboardCaptureModeTest&) = delete;
  GameDashboardCaptureModeTest& operator=(const GameDashboardCaptureModeTest&) =
      delete;
  ~GameDashboardCaptureModeTest() override = default;

  aura::Window* game_window() const { return game_window_.get(); }
  void CloseGameWindow() { game_window_.reset(); }

  // AshTestBase:
  void SetUp() override {
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_RELEASE_TRACK=testimage-channel",
        base::SysInfo::GetLsbReleaseTime());
    AshTestBase::SetUp();
    EXPECT_TRUE(features::IsGameDashboardEnabled());

    game_window_ = CreateAppWindow(gfx::Rect(0, 100, 100, 100));
    game_window_->SetProperty(chromeos::kIsGameKey, true);
  }

  void TearDown() override {
    game_window_.reset();
    AshTestBase::TearDown();
    base::SysInfo::ResetChromeOSVersionInfoForTest();
  }

  CaptureModeController* StartGameCaptureModeSession() {
    auto* controller = CaptureModeController::Get();
    controller->StartForGameDashboard(game_window_.get());
    CHECK(controller->IsActive());
    return controller;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<aura::Window> game_window_;
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

// Tests that when starting the capture mode session from game dashboard, the
// window is pre-selected and won't be altered on mouse hover during the
// session. On the destroying of the pre-selected window, the selected window
// will be reset.
TEST_F(GameDashboardCaptureModeTest, StartForGameDashboardTest) {
  UpdateDisplay("1000x700");
  std::unique_ptr<aura::Window> other_window(
      CreateAppWindow(gfx::Rect(0, 300, 500, 300)));
  CaptureModeController* controller = StartGameCaptureModeSession();
  CaptureModeSession* capture_mode_session = controller->capture_mode_session();
  ASSERT_TRUE(capture_mode_session);
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), game_window());

  // The selected window will not change when mouse hovers on `other_window`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(other_window.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), game_window());

  CloseGameWindow();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(GameDashboardCaptureModeTest, CaptureBar) {
  CaptureModeController* controller = StartGameCaptureModeSession();

  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  auto* start_recording_button = GetStartRecordingButton();
  // Checks that the game capture bar only includes the start recording button,
  // settings button and close button.
  EXPECT_TRUE(start_recording_button);
  EXPECT_FALSE(GetImageToggleButton());
  EXPECT_FALSE(GetVideoToggleButton());
  EXPECT_FALSE(GetFullscreenToggleButton());
  EXPECT_FALSE(GetRegionToggleButton());
  EXPECT_FALSE(GetWindowToggleButton());
  EXPECT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetCloseButton());

  CaptureModeSession* session = controller->capture_mode_session();
  EXPECT_EQ(game_window(), session->GetSelectedWindow());
  // Clicking the start recording button should start the video recording.
  ClickOnView(start_recording_button, GetEventGenerator());
  WaitForRecordingToStart();
  EXPECT_TRUE(controller->is_recording_in_progress());
}

TEST_F(GameDashboardCaptureModeTest, CaptureBarPosition) {
  StartGameCaptureModeSession();
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  const gfx::Rect window_bounds = game_window()->GetBoundsInScreen();
  const gfx::Rect bar_bounds = bar_widget->GetWindowBoundsInScreen();
  // Checks that the game capture bar is inside the window. And centered above a
  // constant distance from the bottom of the window.
  EXPECT_TRUE(window_bounds.Contains(bar_bounds));
  EXPECT_EQ(bar_bounds.CenterPoint().x(), window_bounds.CenterPoint().x());
  EXPECT_EQ(bar_bounds.bottom() + capture_mode::kCaptureBarBottomPadding,
            window_bounds.bottom());
}

}  // namespace ash
