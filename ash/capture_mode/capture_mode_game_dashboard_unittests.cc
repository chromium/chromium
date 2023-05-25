// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/test/ash_test_base.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/base/l10n/l10n_util.h"

namespace message_center {

bool operator==(const ButtonInfo& lhs, const ButtonInfo& rhs) {
  return std::tie(lhs.title, lhs.icon, lhs.placeholder, lhs.type) ==
         std::tie(rhs.title, rhs.icon, rhs.placeholder, rhs.type);
}

}  // namespace message_center

namespace ash {

using ButtonInfo = message_center::ButtonInfo;

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

// Tests that the game dashboard-initiated capture mode session shows the
// notification view with 'Share to YouTube' button and 'delete' buttons.
TEST_F(GameDashboardCaptureModeTest, NotificationView) {
  CaptureModeController* controller = StartGameCaptureModeSession();
  CaptureModeSession* session = controller->capture_mode_session();
  CaptureModeBehavior* active_behavior = session->active_behavior();
  ASSERT_TRUE(active_behavior);
  StartVideoRecordingImmediately();
  CaptureModeTestApi().FlushRecordingServiceForTesting();

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  // Request and wait for a video frame so that the recording service can use it
  // to create a video thumbnail.
  test_delegate->RequestAndWaitForVideoFrame();
  SkBitmap service_thumbnail =
      gfx::Image(test_delegate->GetVideoThumbnail()).AsBitmap();
  EXPECT_FALSE(service_thumbnail.drawsNothing());

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  CaptureNotificationWaiter().Wait();

  const message_center::Notification* notification = GetPreviewNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(notification->image().IsEmpty());

  std::vector<ButtonInfo> expected_buttons_info = {
      ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SHARE_TO_YOUTUBE)),
      ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE))};
  auto actual_buttons_info =
      active_behavior->GetNotificationButtonsInfo(/*for_video=*/true);
  EXPECT_EQ(actual_buttons_info.size(), 2u);
  EXPECT_TRUE(actual_buttons_info == expected_buttons_info);

  const int share_to_youtube_button = 0;
  ClickOnNotification(share_to_youtube_button);
  EXPECT_FALSE(GetPreviewNotification());
}

// Tests that the camera preview widget shows up when starting the game
// dashboard initiated capture mode session for the first time.
TEST_F(GameDashboardCaptureModeTest, CameraPreviewWidgetTest) {
  AddDefaultCamera();
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());

  auto* controller = StartGameCaptureModeSession();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_controller->should_show_preview());
  GetEventGenerator()->MoveMouseToCenterOf(game_window());
  EXPECT_TRUE(camera_controller->camera_preview_widget());

  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(camera_controller->should_show_preview());
  EXPECT_TRUE(camera_controller->camera_preview_widget());

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(camera_controller->should_show_preview());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_F(GameDashboardCaptureModeTest, FocusNavigationOfCaptureBar) {
  UpdateDisplay("1200x1100");
  AddDefaultCamera();
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  auto* controller = StartGameCaptureModeSession();
  ASSERT_TRUE(GetCaptureModeBarWidget());
  EXPECT_EQ(controller->capture_mode_session()->GetSelectedWindow(),
            game_window());
  // Make the selected window large enough to hold collapsible camera preview.
  game_window()->SetBounds({0, 0, 800, 700});

  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  // First tab should focus on the start recording button.
  auto* start_recording_button = GetStartRecordingButton();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kStartRecordingButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  start_recording_button)
                  ->has_focus());

  // Tab again should advance the focus to the camera preview.
  auto* camera_preview_view = camera_controller->camera_preview_view();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Tab again should advance the focus to the resize button inside the camera
  // preview.
  auto* resize_button = camera_preview_view->resize_button();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());

  // Tab again should advance the focus to the settings button.
  auto* settings_button = GetSettingsButton();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(
      CaptureModeSessionFocusCycler::HighlightHelper::Get(settings_button)
          ->has_focus());

  // Tab again should advance the focus to the close button.
  auto* close_button = GetCloseButton();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(close_button)
                  ->has_focus());

  // Shift tab should advance the focus from the close button to the settings
  // button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(
      CaptureModeSessionFocusCycler::HighlightHelper::Get(settings_button)
          ->has_focus());

  // Shift tab again should advance the focus from the settings button to the
  // resize button inside the camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());

  // Shift tab again should advance the focus from the resize button to the
  // camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Shift tab again should advance the focus from the camera preview to the
  // start recording button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kStartRecordingButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  start_recording_button)
                  ->has_focus());
}

TEST_F(GameDashboardCaptureModeTest, GameCaptureModeSessionConfigs) {
  // Verify capture mode session configs for the game dashboard initiated
  // capture session.
  auto* controller = StartGameCaptureModeSession();
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);
  EXPECT_EQ(controller->recording_type(), RecordingType::kWebM);
  EXPECT_EQ(controller->audio_recording_mode(),
            features::IsCaptureModeAudioMixingEnabled()
                ? AudioRecordingMode::kSystemAndMicrophone
                : AudioRecordingMode::kMicrophone);
  EXPECT_EQ(controller->enable_demo_tools(), false);

  // Update the audio recording mode and demo tools configs and stop the
  // session.
  controller->SetAudioRecordingMode(AudioRecordingMode::kSystem);
  controller->EnableDemoTools(true);
  controller->Stop();

  // Start another game dashboard initiated capture mode session and verify
  // that the audio recording mode and demo tools settings are restored from
  // previous session.
  StartGameCaptureModeSession();
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);
  EXPECT_EQ(controller->recording_type(), RecordingType::kWebM);
  EXPECT_EQ(controller->audio_recording_mode(), AudioRecordingMode::kSystem);
  EXPECT_TRUE(controller->enable_demo_tools());
  controller->Stop();

  // Verify that the session configs from the game dashboard initiated capture
  // mode session will not be carried over to the default capture mode session.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
  EXPECT_EQ(controller->recording_type(), RecordingType::kWebM);
  EXPECT_EQ(controller->audio_recording_mode(), AudioRecordingMode::kOff);
  EXPECT_FALSE(controller->enable_demo_tools());
  controller->Stop();
}

}  // namespace ash
