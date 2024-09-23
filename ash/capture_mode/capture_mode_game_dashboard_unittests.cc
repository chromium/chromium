// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/game_dashboard/game_dashboard_context_test_api.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "extensions/common/constants.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/wm/core/window_util.h"

namespace message_center {

bool operator==(const ButtonInfo& lhs, const ButtonInfo& rhs) {
  return std::tie(lhs.title, lhs.icon, lhs.placeholder, lhs.type) ==
         std::tie(rhs.title, rhs.icon, rhs.placeholder, rhs.type);
}

}  // namespace message_center

namespace ash {

namespace {

using ::ui::mojom::CursorType;

}  // namespace

using ButtonInfo = message_center::ButtonInfo;

class GameDashboardCaptureModeTest : public AshTestBase {
 public:
  GameDashboardCaptureModeTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGameDashboard,
                              features::
                                  kFeatureManagementGameDashboardRecordGame},
        /*disabled_features=*/{});
  }
  GameDashboardCaptureModeTest(const GameDashboardCaptureModeTest&) = delete;
  GameDashboardCaptureModeTest& operator=(const GameDashboardCaptureModeTest&) =
      delete;
  ~GameDashboardCaptureModeTest() override = default;

  aura::Window* game_window() const { return game_window_.get(); }
  void CloseGameWindow() { game_window_.reset(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    EXPECT_TRUE(features::IsGameDashboardEnabled());

    // Disable the Game Dashboard welcome dialog for all game windows.
    PrefService* active_user_prefs =
        Shell::Get()->session_controller()->GetActivePrefService();
    ASSERT_TRUE(active_user_prefs);
    active_user_prefs->SetBoolean(prefs::kGameDashboardShowWelcomeDialog,
                                  false);

    game_window_ = CreateAppWindow(gfx::Rect(0, 100, 300, 200));
    game_window_->SetProperty(kAppIDKey,
                              std::string(extension_misc::kGeForceNowAppId));
  }

  void TearDown() override {
    game_window_.reset();
    AshTestBase::TearDown();
  }

  CaptureModeController* StartGameCaptureModeSession() {
    auto* controller = CaptureModeController::Get();
    controller->StartForGameDashboard(game_window_.get());
    CHECK(controller->IsActive());
    return controller;
  }

  // Verifies that the game capture bar is inside the selected game window. And
  // centered above the fixed distance `kCaptureBarBottomPadding` from the
  // bottom of the window.
  void VerifyCaptureBarPosition() {
    views::Widget* bar_widget = GetCaptureModeBarWidget();
    CHECK(bar_widget);
    const gfx::Rect window_bounds = game_window()->GetBoundsInScreen();
    const gfx::Rect bar_bounds = bar_widget->GetWindowBoundsInScreen();
    EXPECT_TRUE(window_bounds.Contains(bar_bounds));
    EXPECT_EQ(bar_bounds.CenterPoint().x(), window_bounds.CenterPoint().x());
    EXPECT_EQ(bar_bounds.bottom() + capture_mode::kGameCaptureBarBottomPadding,
              window_bounds.bottom());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<aura::Window> game_window_;
};

TEST_F(GameDashboardCaptureModeTest, GameDashboardBehavior) {
  CaptureModeController* controller = StartGameCaptureModeSession();
  BaseCaptureModeSession* session = controller->capture_mode_session();
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
      AudioRecordingMode::kSystemAndMicrophone));
  EXPECT_TRUE(active_behavior->ShouldCameraSelectionSettingsBeIncluded());
  EXPECT_FALSE(active_behavior->ShouldDemoToolsSettingsBeIncluded());
  EXPECT_TRUE(active_behavior->ShouldSaveToSettingsBeIncluded());
  EXPECT_FALSE(active_behavior->ShouldGifBeSupported());
  EXPECT_TRUE(active_behavior->ShouldShowPreviewNotification());
  EXPECT_FALSE(active_behavior->ShouldSkipVideoRecordingCountDown());
  EXPECT_FALSE(active_behavior->ShouldCreateAnnotationsOverlayController());
  EXPECT_FALSE(active_behavior->ShouldShowUserNudge());
  EXPECT_TRUE(active_behavior->ShouldAutoSelectFirstCamera());
}

// Tests that a fullscreen screenshot can be taken via the keyboard shortcut
// while a Game-Dashboard-initiated session is active without ending the
// session.
TEST_F(GameDashboardCaptureModeTest, FullscreenScreenshotKeyCombo) {
  StartGameCaptureModeSession();
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  WaitForCaptureFileToBeSaved();
  VerifyActiveBehavior(BehaviorType::kGameDashboard);
}

// Tests that if the user presses the shortcut to switch to default capture
// mode, it is ignored.
TEST_F(GameDashboardCaptureModeTest, SwitchToDefaultCaptureMode) {
  StartGameCaptureModeSession();
  VerifyActiveBehavior(BehaviorType::kGameDashboard);
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  VerifyActiveBehavior(BehaviorType::kGameDashboard);
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
  BaseCaptureModeSession* capture_mode_session =
      controller->capture_mode_session();
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

  BaseCaptureModeSession* session = controller->capture_mode_session();
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

  VerifyCaptureBarPosition();

  // Switching to the tablet mode, the game capture bar should still be inside
  // the window. And centered above the constant distance from the bottom of the
  // window.
  SwitchToTabletMode();
  VerifyCaptureBarPosition();

  // Switching back to the clamshell mode, the game capture bar should be
  // positioned back to the constant distance from the bottom center of the
  // window.
  LeaveTabletMode();
  VerifyCaptureBarPosition();
}

TEST_F(GameDashboardCaptureModeTest, CaptureBarPositionOnDisplayRotation) {
  StartGameCaptureModeSession();
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  VerifyCaptureBarPosition();

  auto* display_manager = Shell::Get()->display_manager();
  const int64_t display_id = WindowTreeHostManager::GetPrimaryDisplayId();

  // Verifies that the capture bar is still at the bottom center position inside
  // the selected window after display rotation.
  for (const auto rotation :
       {display::Display::ROTATE_90, display::Display::ROTATE_180,
        display::Display::ROTATE_270}) {
    display_manager->SetDisplayRotation(display_id, rotation,
                                        display::Display::RotationSource::USER);
    VerifyCaptureBarPosition();
  }
}

// Tests that the game dashboard-initiated capture mode session shows the
// notification view with 'Share to YouTube' button and 'delete' buttons.
TEST_F(GameDashboardCaptureModeTest, NotificationView) {
  CaptureModeController* controller = StartGameCaptureModeSession();
  BaseCaptureModeSession* session = controller->capture_mode_session();
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
  EXPECT_FALSE(controller->can_start_new_recording());
  CaptureNotificationWaiter().Wait();
  EXPECT_TRUE(controller->can_start_new_recording());

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
            AudioRecordingMode::kSystemAndMicrophone);
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

TEST_F(GameDashboardCaptureModeTest, MultiDisplay) {
  UpdateDisplay("800x700,801+0-900x800");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(2u, displays.size());
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 700));
  EXPECT_EQ(displays[1].size(), gfx::Size(900, 800));

  display::Screen* screen = display::Screen::GetScreen();
  auto* controller = StartGameCaptureModeSession();
  BaseCaptureModeSession* capture_mode_session =
      controller->capture_mode_session();
  auto* event_generator = GetEventGenerator();
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(game_window()).id());
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            capture_mode_session->current_root());
  VerifyCaptureBarPosition();
  // The current root window should not change if moving the cursor to a
  // different display as the game window.
  event_generator->MoveMouseTo(displays[1].bounds().CenterPoint());
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            capture_mode_session->current_root());

  // Using the shortcut ALT+SEARCH+M to move the window to another display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  // Verifies that the capture bar and the current root window of the capture
  // mode session are updated correctly after moving the game window to another
  // display.
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(game_window()).id());
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            capture_mode_session->current_root());
  VerifyCaptureBarPosition();
  // The current root window should not change if moving the cursor to a
  // different display as the game window.
  event_generator->MoveMouseTo(displays[0].bounds().CenterPoint());
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            capture_mode_session->current_root());

  // Using the shortcut ALT+SEARCH+M to move the window back to the previous
  // display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  // Verifies the capture bar and the current root window after moving the game
  // window back to the previous display.
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(game_window()).id());
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            capture_mode_session->current_root());
  VerifyCaptureBarPosition();
}

TEST_F(GameDashboardCaptureModeTest, NoLabelInTabletMode) {
  auto* controller = StartGameCaptureModeSession();

  BaseCaptureModeSession* session = controller->capture_mode_session();
  ASSERT_EQ(game_window(), session->GetSelectedWindow());

  SwitchToTabletMode();

  views::Label* label_internal_view =
      CaptureModeSessionTestApi(session).GetCaptureLabelInternalView();
  EXPECT_FALSE(label_internal_view->GetVisible());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightFullAboveBar) {
  UpdateDisplay("1000x500");
  game_window()->SetBounds(gfx::Rect(0, 50, 500, 450));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // There is space for the settings menu to take up its entire preferred size
  // above the bar view.
  EXPECT_GE(settings_bounds.y(),
            capture_mode::kMinDistanceFromSettingsToScreen);
  EXPECT_EQ(settings_bounds.height(),
            test_api.GetCaptureModeSettingsView()->GetPreferredSize().height());
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightFullBelowBar) {
  UpdateDisplay("1000x500");
  game_window()->SetBounds(gfx::Rect(0, 0, 500, 100));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // There is space for the settings menu to take up its entire preferred size
  // below the bar view, but not above it.
  const auto bar_bounds = test_api.GetCaptureModeBarView()->GetBoundsInScreen();
  EXPECT_EQ(settings_bounds.y(),
            bar_bounds.bottom() +
                capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu);
  EXPECT_GE(settings_bounds.y(),
            capture_mode::kMinDistanceFromSettingsToScreen);
  EXPECT_EQ(settings_bounds.height(),
            test_api.GetCaptureModeSettingsView()->GetPreferredSize().height());
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightConstrainedAboveBar) {
  UpdateDisplay("1000x250");
  game_window()->SetBounds(gfx::Rect(0, 50, 500, 200));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // The menu height has been constrained, but has not reached its minimum size.
  EXPECT_EQ(settings_bounds.y(),
            capture_mode::kMinDistanceFromSettingsToScreen);
  EXPECT_LT(settings_bounds.height(),
            test_api.GetCaptureModeSettingsView()->GetPreferredSize().height());
  EXPECT_GT(settings_bounds.height(), capture_mode::kSettingsMenuMinHeight);
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightConstrainedBelowBar) {
  UpdateDisplay("1000x500");
  game_window()->SetBounds(gfx::Rect(0, 0, 500, 200));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // The menu height has been constrained, but has not reached its minimum size.
  const auto bar_bounds = test_api.GetCaptureModeBarView()->GetBoundsInScreen();
  EXPECT_EQ(settings_bounds.y(),
            bar_bounds.bottom() +
                capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu);
  EXPECT_LT(settings_bounds.height(),
            test_api.GetCaptureModeSettingsView()->GetPreferredSize().height());
  EXPECT_GT(settings_bounds.height(), capture_mode::kSettingsMenuMinHeight);
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightMinimumAboveBar) {
  UpdateDisplay("1000x100");
  game_window()->SetBounds(gfx::Rect(0, 50, 500, 50));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // The menu is at its minimum size and height above the bar view.
  EXPECT_EQ(settings_bounds.y(),
            capture_mode::kMinDistanceFromSettingsToScreen);
  EXPECT_EQ(settings_bounds.height(), capture_mode::kSettingsMenuMinHeight);
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, StopOnWindowSentToDifferentDesk) {
  NewDesk();
  auto* controller = StartGameCaptureModeSession();

  // Send the window to a different desk using the accelerator.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(GameDashboardCaptureModeTest, StopOnActiveDeskChanged) {
  NewDesk();
  auto* controller = StartGameCaptureModeSession();

  // Switch to the new desk using the accelerator.
  DeskSwitchAnimationWaiter waiter;
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  waiter.Wait();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(GameDashboardCaptureModeTest, SettingsMenuHeightMinimumBelowBar) {
  UpdateDisplay("1000x100");
  game_window()->SetBounds(gfx::Rect(0, 0, 500, 50));
  StartGameCaptureModeSession();
  BaseCaptureModeSession* session =
      CaptureModeController::Get()->capture_mode_session();
  ASSERT_TRUE(session);

  // Open the settings menu.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSessionTestApi test_api(session);
  ASSERT_TRUE(test_api.GetCaptureModeSettingsWidget());

  const gfx::Rect settings_bounds =
      test_api.GetCaptureModeSettingsWidget()->GetWindowBoundsInScreen();

  // The menu is at its minimum size and height below the bar view.
  const auto bar_bounds = test_api.GetCaptureModeBarView()->GetBoundsInScreen();
  EXPECT_EQ(settings_bounds.y(),
            bar_bounds.bottom() +
                capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu);
  EXPECT_EQ(settings_bounds.height(), capture_mode::kSettingsMenuMinHeight);
  EXPECT_EQ(
      settings_bounds.size(),
      CaptureModeSettingsTestApi().GetSettingsView()->GetVisibleRect().size());
}

TEST_F(GameDashboardCaptureModeTest, GameCaptureModeRecordInstantlyTest) {
  AddDefaultCamera();

  // Start a game dashboard initiated capture mode session and check the initial
  // configs for game dashboard initiated capture mode.
  auto* controller = StartGameCaptureModeSession();
  EXPECT_FALSE(controller->enable_demo_tools());
  EXPECT_EQ(controller->audio_recording_mode(),
            AudioRecordingMode::kSystemAndMicrophone);

  auto* camera_controller = controller->camera_controller();
  ASSERT_TRUE(camera_controller->camera_preview_widget());
  const CameraId camera_id(kDefaultCameraModelId, 1);
  EXPECT_EQ(camera_id, camera_controller->selected_camera());

  // Update the audio recording mode and demo tools configs for the
  // game-dashboard initiated capture mode.
  controller->SetAudioRecordingMode(AudioRecordingMode::kOff);
  controller->EnableDemoTools(true);
  controller->Stop();

  // Start the recording with the game dashboard instant recording API.
  controller->StartRecordingInstantlyForGameDashboard(game_window());

  // Verify that recording starts immediately.
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Verify that the game dashboard capture mode configs are not overwritten.
  CaptureModeBehavior* behavior =
      CaptureModeTestApi().GetBehavior(BehaviorType::kGameDashboard);
  const auto capture_mode_configs = behavior->capture_mode_configs();
  EXPECT_EQ(capture_mode_configs.audio_recording_mode,
            AudioRecordingMode::kOff);
  EXPECT_TRUE(capture_mode_configs.demo_tools_enabled);

  // Verify that the configs in `CaptureModeController` are restored.
  EXPECT_EQ(controller->audio_recording_mode(), AudioRecordingMode::kOff);
  EXPECT_FALSE(controller->enable_demo_tools());

  // Verify that selfie camera is visible and is parented correctly to the game
  // window.
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  ASSERT_TRUE(camera_preview_widget);
  EXPECT_EQ(camera_preview_widget->GetNativeWindow()->parent(), game_window());
}

TEST_F(GameDashboardCaptureModeTest, UserTurnsOffCamera) {
  AddDefaultCamera();

  // By default, the first available camera should be auto selected.
  auto* controller = StartGameCaptureModeSession();
  auto* camera_controller = controller->camera_controller();
  ASSERT_TRUE(camera_controller->camera_preview_widget());
  const CameraId camera_id(kDefaultCameraModelId, 1);
  EXPECT_EQ(camera_id, camera_controller->selected_camera());

  // Now, open the settings menu, and select the "camera off" option.
  LeftClickOn(GetSettingsButton());
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* camera_menu_group = test_api.GetCameraMenuGroup();
  ASSERT_TRUE(camera_menu_group);
  EXPECT_TRUE(camera_menu_group->GetVisible());
  auto* off_option = test_api.GetCameraOption(kCameraOff);
  EXPECT_TRUE(off_option);
  LeftClickOn(off_option);

  // No camera should be selected, and the preview widget should be closed.
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Close the session and start a new one. No camera should be auto selected.
  controller->Stop();
  StartGameCaptureModeSession();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
  controller->Stop();

  // Start a new default session and select a camera, then end the session.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  camera_controller->SetSelectedCamera(camera_id);
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  controller->Stop();

  // When we start a new Game Dashboard session next, the camera selected from
  // the previous default session will remain selected.
  StartGameCaptureModeSession();
  EXPECT_EQ(camera_id, camera_controller->selected_camera());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  controller->Stop();
}

TEST_F(GameDashboardCaptureModeTest, StartWithNoCamera) {
  // Initially there's no camera connected, so the Game Dashboard auto camera
  // selection won't work.
  auto* controller = StartGameCaptureModeSession();
  auto* camera_controller = controller->camera_controller();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Now stop the current session, connect a camera, and start a new Game
  // Dashboard session. This new session should be able to auto-select the
  // camera.
  controller->Stop();
  AddDefaultCamera();
  StartGameCaptureModeSession();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  const CameraId camera_id(kDefaultCameraModelId, 1);
  EXPECT_EQ(camera_id, camera_controller->selected_camera());
  controller->Stop();

  // The next Game Dashboard session should still launch with a camera.
  StartGameCaptureModeSession();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_EQ(camera_id, camera_controller->selected_camera());
}

TEST_F(GameDashboardCaptureModeTest, CameraAutoSelectionDisabledOnChange) {
  const std::string device_id_1 = "/dev/video0";
  const std::string display_name_1 = "Integrated Webcam";

  const std::string device_id_2 = "/dev/video1";
  const std::string display_name_2 = "Integrated Webcam 1";

  AddFakeCamera(device_id_1, display_name_1, display_name_1);
  AddFakeCamera(device_id_2, display_name_2, display_name_2);

  // The first Game Dashboard session, the first camera will be auto selected.
  auto* controller = StartGameCaptureModeSession();
  auto* camera_controller = controller->camera_controller();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  const CameraId camera_id1(display_name_1, 1);
  EXPECT_EQ(camera_id1, camera_controller->selected_camera());

  // Now, simulate a change by the user to select a different camera while the
  // session is still running.
  const CameraId camera_id2(display_name_2, 1);
  camera_controller->SetSelectedCamera(camera_id2);
  EXPECT_EQ(camera_id2, camera_controller->selected_camera());

  // Stop the session and expect that the camera remains selected.
  controller->Stop();
  EXPECT_EQ(camera_id2, camera_controller->selected_camera());
}

TEST_F(GameDashboardCaptureModeTest, NoDimmingOfGameDashboardWidgets) {
  auto* controller = CaptureModeController::Get();
  controller->StartRecordingInstantlyForGameDashboard(game_window());
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  ASSERT_EQ(game_window(), recording_watcher->window_being_recorded());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(game_window()));

  // The window that hosts the game dashboard button should not be dimmed.
  GameDashboardContextTestApi context_test_api{
      GameDashboardController::Get()->GetGameDashboardContext(game_window()),
      GetEventGenerator()};
  auto* game_dashboard_button_widget =
      context_test_api.GetGameDashboardButtonWidget();
  ASSERT_TRUE(game_dashboard_button_widget);
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(
      game_dashboard_button_widget->GetNativeWindow()));

  // Open the game dashboard menu, and expect that the window hosting the menu
  // is not dimmed either.
  context_test_api.OpenTheMainMenu();
  auto* game_dashboard_menu_widget = context_test_api.GetMainMenuWidget();
  ASSERT_TRUE(game_dashboard_menu_widget);
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(
      game_dashboard_menu_widget->GetNativeWindow()));

  // Finally, the toolbar widget should also not be dimmed.
  context_test_api.OpenTheToolbar();
  auto* game_dashboard_toolbar_widget = context_test_api.GetToolbarWidget();
  ASSERT_TRUE(game_dashboard_toolbar_widget);
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(
      game_dashboard_toolbar_widget->GetNativeWindow()));
}

TEST_F(GameDashboardCaptureModeTest, AvoidToolbarAndCameraPreviewIntersection) {
  UpdateDisplay("1200x1100");
  AddDefaultCamera();

  // Make the game window large enough to show a camera preview.
  game_window()->SetBounds({50, 50, 1100, 1000});

  auto* controller = CaptureModeController::Get();
  controller->StartRecordingInstantlyForGameDashboard(game_window());

  auto* camera_controller = controller->camera_controller();
  ASSERT_TRUE(camera_controller->should_show_preview());

  // The window that hosts the game dashboard button should not be dimmed.
  GameDashboardContextTestApi context_test_api{
      GameDashboardController::Get()->GetGameDashboardContext(game_window()),
      GetEventGenerator()};

  // Open the game dashboard toolbar.
  context_test_api.OpenTheMainMenu();
  context_test_api.OpenTheToolbar();
  context_test_api.CloseTheMainMenu();
  auto* game_dashboard_toolbar_widget = context_test_api.GetToolbarWidget();
  ASSERT_TRUE(game_dashboard_toolbar_widget);

  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  ASSERT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  auto* toolbar_window = game_dashboard_toolbar_widget->GetNativeWindow();

  // Verify that the toolbar and camera preview do not overlap initially.
  EXPECT_FALSE(preview_window->GetBoundsInScreen().Intersects(
      toolbar_window->GetBoundsInScreen()));

  // Drag the camera preview widget and drop it on the toolbar, it should find a
  // different place to exist.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      preview_window->GetBoundsInScreen().CenterPoint());
  event_generator->DragMouseTo(
      toolbar_window->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(preview_window->GetBoundsInScreen().Intersects(
      toolbar_window->GetBoundsInScreen()));

  // Now drag the toolbar and drop it on top of the camera preview. The camera
  // preview should move out of the way.
  auto preview_bounds_before_dragging_toolbar =
      preview_window->GetBoundsInScreen();
  event_generator->MoveMouseTo(
      toolbar_window->GetBoundsInScreen().CenterPoint());
  event_generator->DragMouseTo(
      preview_window->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(preview_window->GetBoundsInScreen().Intersects(
      toolbar_window->GetBoundsInScreen()));
  EXPECT_NE(preview_window->GetBoundsInScreen(),
            preview_bounds_before_dragging_toolbar);
}

TEST_F(GameDashboardCaptureModeTest, CursorAndClickBehaviorWhenAnchored) {
  // Create second window on screen that underlaps `game_window_` slightly.
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(50, 150, 100, 100)));

  // The game window should be the top most active window.
  wm::ActivateWindow(game_window());
  auto* controller = StartGameCaptureModeSession();

  // Hover over empty space where there is no window.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  // Clicking should not start a recording.
  event_generator->ClickLeftButton();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());

  // Hover over the non-game window.
  event_generator->MoveMouseTo(gfx::Point(125, 225));
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->ClickLeftButton();
  // Clicking should not start a recording.
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());

  // Hover over the anchored window where it overlaps with the non-game window.
  event_generator->MoveMouseTo(gfx::Point(75, 175));
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(
      CaptureModeSessionTestApi().IsUsingCustomCursor(CaptureModeType::kVideo));
  // Start recording by clicking on the anchored window.
  event_generator->ClickLeftButton();
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

// -----------------------------------------------------------------------------
// GameDashboardCaptureModeHistogramTest:
// Test fixture to verify game dashboard initiated screen capture histograms
// depending on the test param (true for tablet mode, false for clamshell mode).

class GameDashboardCaptureModeHistogramTest
    : public GameDashboardCaptureModeTest,
      public ::testing::WithParamInterface<bool> {
 public:
  GameDashboardCaptureModeHistogramTest() = default;
  ~GameDashboardCaptureModeHistogramTest() override = default;

  // GameDashboardCaptureModeTest:
  void SetUp() override {
    GameDashboardCaptureModeTest::SetUp();
    if (GetParam()) {
      SwitchToTabletMode();
    }
  }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameCaptureConfigurationHistogram) {
  constexpr char kCaptureConfigurationBase[] = "CaptureConfiguration";
  CaptureModeTestApi test_api;
  const std::string histogram_name =
      BuildHistogramName(kCaptureConfigurationBase,
                         test_api.GetBehavior(BehaviorType::kGameDashboard),
                         /*append_ui_mode_suffix=*/true);
  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kWindowRecording, 0);

  auto* controller = StartGameCaptureModeSession();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  test_api.StopVideoRecording();
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kWindowRecording, 1);
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameCaptureConfigurationHistogramForInstantScreenshot) {
  constexpr char kCaptureConfigurationBase[] = "CaptureConfiguration";
  CaptureModeTestApi test_api;
  const std::string histogram_name =
      BuildHistogramName(kCaptureConfigurationBase,
                         test_api.GetBehavior(BehaviorType::kGameDashboard),
                         /*append_ui_mode_suffix=*/true);
  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kWindowScreenshot, 0);

  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(game_window());
  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kWindowScreenshot, 1);
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameScreenRecordingLengthHistogram) {
  constexpr char kRecordLenthHistogramBase[] = "ScreenRecordingLength";

  auto* controller = StartGameCaptureModeSession();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  WaitForSeconds(/*seconds=*/1);

  CaptureModeTestApi test_api;
  test_api.StopVideoRecording();
  EXPECT_FALSE(controller->is_recording_in_progress());
  WaitForCaptureFileToBeSaved();

  histogram_tester_.ExpectUniqueSample(
      BuildHistogramName(kRecordLenthHistogramBase,
                         test_api.GetBehavior(BehaviorType::kGameDashboard),
                         /*append_ui_mode_suffix=*/true),
      /*sample=*/1, /*expected_bucket_count=*/1);
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameScreenRecordingFileSizeHistogram) {
  constexpr char kHistogramNameBase[] = "ScreenRecordingFileSize";

  CaptureModeTestApi test_api;
  const auto histogram_name = BuildHistogramName(
      kHistogramNameBase, test_api.GetBehavior(BehaviorType::kGameDashboard),
      /*append_ui_mode_suffix=*/true);
  histogram_tester_.ExpectTotalCount(histogram_name, /*expected_count=*/0);

  StartGameCaptureModeSession();
  StartVideoRecordingImmediately();
  test_api.StopVideoRecording();
  WaitForCaptureFileToBeSaved();
  histogram_tester_.ExpectTotalCount(histogram_name,
                                     /*expected_count=*/1);
}

TEST_P(GameDashboardCaptureModeHistogramTest, GameSaveToLocationHistogram) {
  constexpr char kHistogramNameBase[] = "SaveLocation";

  CaptureModeTestApi test_api;
  const auto histogram_name = BuildHistogramName(
      kHistogramNameBase, test_api.GetBehavior(BehaviorType::kGameDashboard),
      /*append_ui_mode_suffix=*/true);

  auto* test_delegate = CaptureModeController::Get()->delegate_for_testing();

  // Initialize four different save-to locations for screen capture that
  // includes default downloads folder, local customized folder, root drive and
  // a specific folder on drive.
  const auto downloads_folder = test_delegate->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder =
      CreateCustomFolderInUserDownloadsPath("test");
  base::FilePath mount_point_path;
  test_delegate->GetDriveFsMountPointPath(&mount_point_path);
  const auto root_drive_folder = mount_point_path.Append("root");
  const base::FilePath non_root_drive_folder = CreateFolderOnDriveFS("test");

  struct {
    base::FilePath set_save_file_folder;
    CaptureModeSaveToLocation save_location;
  } kTestCases[] = {
      {downloads_folder, CaptureModeSaveToLocation::kDefault},
      {custom_folder, CaptureModeSaveToLocation::kCustomizedFolder},
      {root_drive_folder, CaptureModeSaveToLocation::kDrive},
      {non_root_drive_folder, CaptureModeSaveToLocation::kDriveFolder},
  };

  for (auto test_case : kTestCases) {
    histogram_tester_.ExpectBucketCount(histogram_name, test_case.save_location,
                                        0);
    auto* controller = StartGameCaptureModeSession();
    controller->SetCustomCaptureFolder(test_case.set_save_file_folder);
    StartVideoRecordingImmediately();
    test_api.StopVideoRecording();
    auto file_saved_path = WaitForCaptureFileToBeSaved();
    histogram_tester_.ExpectBucketCount(histogram_name, test_case.save_location,
                                        1);
  }
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameRecordingStartsWithCameraHistogram) {
  UpdateDisplay("1000x700");
  constexpr char kHistogramNameBase[] = "RecordingStartsWithCamera";
  AddDefaultCamera();

  for (const auto camera_on : {true, false}) {
    CaptureModeTestApi test_api;
    const std::string histogram_name = BuildHistogramName(
        kHistogramNameBase, test_api.GetBehavior(BehaviorType::kGameDashboard),
        /*append_ui_mode_suffix=*/true);
    histogram_tester_.ExpectBucketCount(histogram_name, camera_on, 0);

    auto* controller = StartGameCaptureModeSession();
    EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
    auto* camera_controller = controller->camera_controller();
    if (!camera_on) {
      camera_controller->SetSelectedCamera(CameraId());
    }
    test_api.PerformCapture();
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());

    test_api.StopVideoRecording();
    EXPECT_FALSE(controller->is_recording_in_progress());
    WaitForCaptureFileToBeSaved();
    histogram_tester_.ExpectBucketCount(histogram_name, camera_on, 1);
  }
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameDemoToolsEnabledOnRecordingHistogram) {
  constexpr char kHistogramNameBase[] = "DemoToolsEnabledOnRecordingStart";
  CaptureModeTestApi test_api;
  for (const auto enable_demo_tools : {false, true}) {
    const auto histogram_name = BuildHistogramName(
        kHistogramNameBase, test_api.GetBehavior(BehaviorType::kGameDashboard),
        /*append_ui_mode_suffix=*/true);
    histogram_tester_.ExpectBucketCount(histogram_name, enable_demo_tools, 0);
    auto* controller = StartGameCaptureModeSession();
    controller->EnableDemoTools(enable_demo_tools);
    StartVideoRecordingImmediately();
    test_api.StopVideoRecording();
    WaitForCaptureFileToBeSaved();
    histogram_tester_.ExpectBucketCount(histogram_name, enable_demo_tools, 1);
  }
}

TEST_P(GameDashboardCaptureModeHistogramTest, GameAudioRecordingModeHistogram) {
  constexpr char kHistogramNameBase[] = "AudioRecordingMode";
  CaptureModeTestApi test_api;
  for (const auto audio_mode :
       {AudioRecordingMode::kOff, AudioRecordingMode::kMicrophone,
        AudioRecordingMode::kSystem,
        AudioRecordingMode::kSystemAndMicrophone}) {
    const auto histogram_name = BuildHistogramName(
        kHistogramNameBase, test_api.GetBehavior(BehaviorType::kGameDashboard),
        /*append_ui_mode_suffix=*/true);
    histogram_tester_.ExpectBucketCount(histogram_name, audio_mode, 0);
    auto* controller = StartGameCaptureModeSession();
    controller->SetAudioRecordingMode(audio_mode);
    controller->Stop();
    histogram_tester_.ExpectBucketCount(histogram_name, audio_mode, 1);
  }
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       GameDashboardEndRecordingReasonHistogram) {
  constexpr char kHistogramNameBase[] = "EndRecordingReason";

  CaptureModeTestApi test_api;

  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, test_api.GetBehavior(BehaviorType::kDefault),
      /*append_ui_mode_suffix=*/true);

  // Testing the game dashboard stop recording button enum.
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      /*sample=*/EndRecordingReason::kGameDashboardStopRecordingButton,
      /*expected_count=*/0);
  StartGameCaptureModeSession();
  StartVideoRecordingImmediately();
  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kGameDashboardStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      /*sample=*/EndRecordingReason::kGameDashboardStopRecordingButton,
      /*expected_count=*/1);

  // Testing the game toolbar stop recording button enum.
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      /*sample=*/EndRecordingReason::kGameToolbarStopRecordingButton,
      /*expected_count=*/0);
  StartGameCaptureModeSession();
  StartVideoRecordingImmediately();
  CaptureModeController::Get()->EndVideoRecording(
      EndRecordingReason::kGameToolbarStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      /*sample=*/EndRecordingReason::kGameToolbarStopRecordingButton,
      /*expected_count=*/1);

  // Testing stop recording by the tablet mode enum.
  // Game dashboard is not available on the tablet mode.
  if (GetParam()) {
    return;
  }

  histogram_tester_.ExpectBucketCount(
      histogram_name,
      /*sample=*/EndRecordingReason::kGameDashboardTabletMode,
      /*expected_count=*/0);
  auto game_dashboard_test_api = std::make_unique<GameDashboardContextTestApi>(
      GameDashboardController::Get()->GetGameDashboardContext(game_window()),
      GetEventGenerator());

  game_dashboard_test_api->OpenTheMainMenu();
  LeftClickOn(game_dashboard_test_api->GetMainMenuRecordGameTile());
  // Clicking on the record game tile closes the main menu, and asynchronously
  // starts the capture session. Run until idle to ensure that the posted task
  // runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  LeftClickOn(GetStartRecordingButton());
  WaitForRecordingToStart();
  EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  TabletModeControllerTestApi().EnterTabletMode();
  WaitForCaptureFileToBeSaved();
  // The histogram name becomes
  // "ash.CaptureModeController.EndRecordingReason.TabletMode";
  histogram_tester_.ExpectBucketCount(
      BuildHistogramName(kHistogramNameBase,
                         test_api.GetBehavior(BehaviorType::kDefault),
                         /*append_ui_mode_suffix=*/true),
      /*sample=*/EndRecordingReason::kGameDashboardTabletMode,
      /*expected_count=*/1);
}

TEST_P(GameDashboardCaptureModeHistogramTest,
       CaptureScreenshotOfGivenWindowMetric) {
  constexpr char kHistogramNameBase[] = "SaveLocation";
  const base::FilePath custom_folder =
      CreateCustomFolderInUserDownloadsPath("test");
  const auto histogram_name = BuildHistogramName(
      kHistogramNameBase,
      CaptureModeTestApi().GetBehavior(BehaviorType::kGameDashboard),
      /*append_ui_mode_suffix=*/true);

  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeSaveToLocation::kCustomizedFolder, 0);
  CaptureModeController* controller = CaptureModeController::Get();
  controller->SetCustomCaptureFolder(custom_folder);
  controller->CaptureScreenshotOfGivenWindow(game_window());
  const auto file_saved_path = WaitForCaptureFileToBeSaved();
  histogram_tester_.ExpectBucketCount(
      histogram_name, CaptureModeSaveToLocation::kCustomizedFolder, 1);
}

// Tests that the entry point metrics are recorded correctly for game dashboard
// initiated capture mode both with and without a capture mode session.
TEST_P(GameDashboardCaptureModeHistogramTest, EntryPointTest) {
  constexpr char kCaptureConfigurationBase[] = "EntryPoint";
  CaptureModeTestApi test_api;
  const std::string histogram_name =
      BuildHistogramName(kCaptureConfigurationBase, nullptr,
                         /*append_ui_mode_suffix=*/true);
  histogram_tester_.ExpectBucketCount(histogram_name,
                                      CaptureModeEntryType::kGameDashboard, 0);

  auto* controller = StartGameCaptureModeSession();
  controller->Stop();
  histogram_tester_.ExpectBucketCount(histogram_name,
                                      CaptureModeEntryType::kGameDashboard, 1);

  controller->StartRecordingInstantlyForGameDashboard(game_window());
  histogram_tester_.ExpectBucketCount(histogram_name,
                                      CaptureModeEntryType::kGameDashboard, 2);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GameDashboardCaptureModeHistogramTest,
                         ::testing::Bool());

}  // namespace ash
