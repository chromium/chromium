// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/linux_apps_bubble_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kToggleButtonHistogramName[] =
    "Ash.VideoConferenceTray.ToggleBubbleButton.Click";
constexpr char kCameraMuteHistogramName[] =
    "Ash.VideoConferenceTray.CameraMuteButton.Click";
constexpr char kMicrophoneMuteHistogramName[] =
    "Ash.VideoConferenceTray.MicrophoneMuteButton.Click";
constexpr char kStopScreenShareHistogramName[] =
    "Ash.VideoConferenceTray.StopScreenShareButton.Click";
constexpr char kTrayBackgroundViewHistogramName[] =
    "Ash.StatusArea.TrayBackgroundView.Pressed";

constexpr base::TimeDelta kGetMediaAppsDelayTime = base::Milliseconds(100);

void SetSessionState(session_manager::SessionState state) {
  ash::SessionInfo info;
  info.state = state;
  ash::Shell::Get()->session_controller()->SetSessionInfo(info);
}

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// A customized controller that will mock a delay for `GetMediaApps()`. We might
// have this delay when getting lacros media apps.
class DelayVideoConferenceTrayController
    : public ash::FakeVideoConferenceTrayController {
 public:
  DelayVideoConferenceTrayController() = default;
  DelayVideoConferenceTrayController(
      const DelayVideoConferenceTrayController&) = delete;
  DelayVideoConferenceTrayController& operator=(
      const DelayVideoConferenceTrayController&) = delete;
  ~DelayVideoConferenceTrayController() override = default;

  // ash::FakeVideoConferenceTrayController:
  void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback) override {
    getting_media_apps_called_++;
    MediaApps apps;
    for (auto& app : media_apps()) {
      apps.push_back(app->Clone());
    }
    timer_.Start(
        FROM_HERE, kGetMediaAppsDelayTime,
        base::BindOnce(
            [](base::OnceCallback<void(MediaApps)> ui_callback,
               MediaApps apps) { std::move(ui_callback).Run(std::move(apps)); },
            std::move(ui_callback), std::move(apps)));
  }

  int getting_media_apps_called() { return getting_media_apps_called_; }

 private:
  base::OneShotTimer timer_;

  int getting_media_apps_called_ = 0;
};

}  // namespace

namespace ash {

class VideoConferenceTrayTest : public AshTestBase {
 public:
  VideoConferenceTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  VideoConferenceTrayTest(const VideoConferenceTrayTest&) = delete;
  VideoConferenceTrayTest& operator=(const VideoConferenceTrayTest&) = delete;
  ~VideoConferenceTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVcStopAllScreenShare,
         features::kFeatureManagementVideoConference},
        {});

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();
  }

  void TearDown() override {
    num_media_apps_simulated_ = 0;

    AshTestBase::TearDown();
    controller_.reset();
  }

  VideoConferenceTray* GetSecondaryVideoConferenceTray() {
    Shelf* const shelf =
        Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
            ->shelf();
    return shelf->status_area_widget()->video_conference_tray();
  }

  // Convenience function to create `num_apps` media apps.
  void CreateMediaApps(int num_apps,
                       bool clear_existing_apps = true,
                       crosapi::mojom::VideoConferenceAppType app_type =
                           crosapi::mojom::VideoConferenceAppType::kChromeApp) {
    if (clear_existing_apps) {
      controller()->ClearMediaApps();
    }

    auto* title = u"Meet";
    const std::string kMeetTestUrl = "https://meet.google.com/abc-xyz/ab-123";
    for (int i = 0; i < num_apps; i++) {
      controller()->AddMediaApp(
          crosapi::mojom::VideoConferenceMediaAppInfo::New(
              /*id=*/base::UnguessableToken::Create(),
              /*last_activity_time=*/base::Time::Now(),
              /*is_capturing_camera=*/true,
              /*is_capturing_microphone=*/true, /*is_capturing_screen=*/true,
              title,
              /*url=*/GURL(kMeetTestUrl), /*app_type=*/app_type));
    }
  }

  // Sets shelf autohide behavior and creates a window to hide the shelf.
  // Destroying `widget` will result in the shelf showing.
  std::unique_ptr<views::Widget> ForceShelfToAutoHideOnPrimaryDisplay() {
    Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
    shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
    // Create a normal unmaximized window; the shelf should then hide.
    std::unique_ptr<views::Widget> widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget->SetBounds(gfx::Rect(0, 0, 100, 100));

    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

    return widget;
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  VideoConferenceTrayButton* camera_icon() {
    return video_conference_tray()->camera_icon();
  }

  VideoConferenceTrayButton* audio_icon() {
    return video_conference_tray()->audio_icon();
  }

  VideoConferenceTrayButton* screen_share_icon() {
    return video_conference_tray()->screen_share_icon();
  }

  // Make the tray and buttons visible by setting `VideoConferenceMediaState`,
  // and return the state so it can be modified.
  VideoConferenceMediaState SetTrayAndButtonsVisible() {
    VideoConferenceMediaState state;
    state.has_media_app = true;
    state.has_camera_permission = true;
    state.has_microphone_permission = true;
    state.is_capturing_screen = true;
    controller()->UpdateWithMediaState(state);
    return state;
  }

  // Simulates adding or removing (depending on `add`) a single app that is
  // capturing camera or mic. `CreateMediaApps()` updates the media state, and
  // `OnAppUpdated()` is called by the backend when a new app starts capturing.
  void ModifyAppsCapturing(bool add) {
    if (add) {
      CreateMediaApps(/*num_apps=*/++num_media_apps_simulated_,
                      /*clear_existing_apps=*/false);
      // `VideoConferenceTrayController::HandleClientUpdate()` is triggered via
      // mojo, this directly calls `OnAppAdded()`.
      controller_->OnAppAdded();
    } else {
      CreateMediaApps(--num_media_apps_simulated_,
                      /*clear_existing_apps=*/false);
    }
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  int num_media_apps_simulated_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayTest, ClickTrayButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(video_conference_tray()->GetBubbleView());

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, true, 1);

  // Clicking it again should reset the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, false, 1);

  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());
  histogram_tester.ExpectBucketCount(kToggleButtonHistogramName, true, 2);

  // Click anywhere else outside the bubble (i.e. the status area button) should
  // close the bubble.
  LeftClickOn(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
}

// Makes sure metrics are recorded for the video conference tray or any nested
// button being pressed.
TEST_F(VideoConferenceTrayTest, TrayPressedMetrics) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  LeftClickOn(toggle_bubble_button());
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 1);

  LeftClickOn(camera_icon());
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 2);

  LeftClickOn(audio_icon());
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 3);

  LeftClickOn(screen_share_icon());
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 4);
}

// Tests that tapping directly on the VideoConferenceTray (not the child toggle
// buttons) toggles the bubble.
TEST_F(VideoConferenceTrayTest, ClickTrayBackgroundViewTogglesBubble) {
  // Make sure all buttons in the tray are visible.
  SetTrayAndButtonsVisible();

  // Tap the body of the TrayBackgroundView, missing all toggle buttons. The
  // bubble should show up.
  GetEventGenerator()->GestureTapAt(
      toggle_bubble_button()->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(4, 0));

  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(toggle_bubble_button()->toggled());

  // Tap the body again, it should hide the bubble.
  GetEventGenerator()->GestureTapAt(
      toggle_bubble_button()->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(4, 0));

  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
}

TEST_F(VideoConferenceTrayTest, ToggleBubbleButtonRotation) {
  SetTrayAndButtonsVisible();

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  // When the bubble is not open in horizontal shelf, the indicator should point
  // up (not rotated).
  EXPECT_EQ(0,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in horizontal shelf, the indicator should point
  // down.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(180,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  // When the bubble is not open in left shelf, the indicator should point to
  // the right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in left shelf, the indicator should point to the
  // left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  // When the bubble is not open in right shelf, the indicator should point to
  // the left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in right shelf, the indicator should point to the
  // right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());
}

// Makes sure that the tray does not animate to new inkdrop state when
// activated, which is the default behavior of `TrayBackgroundView`.
TEST_F(VideoConferenceTrayTest, ToggleBubbleInkdrop) {
  auto* ink_drop = views::InkDrop::Get(video_conference_tray())->GetInkDrop();

  SetTrayAndButtonsVisible();
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Open bubble, the tray should not install inkdrop.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Close the bubble, inkdrop should still be hidden.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
}

TEST_F(VideoConferenceTrayTest, TrayVisibility) {
  // We only show the tray when there are any running media app(s).
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(video_conference_tray()->GetVisible());
  EXPECT_TRUE(audio_icon()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());

  // At first, the tray, as well as audio and camera icons should still be
  // visible.
  EXPECT_TRUE(video_conference_tray()->GetVisible());
  EXPECT_TRUE(audio_icon()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());

  state.has_media_app = false;
  state.has_camera_permission = false;
  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(video_conference_tray()->GetVisible());
  EXPECT_FALSE(audio_icon()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, TrayVisibilityOnSecondaryDisplay) {
  UpdateDisplay("800x700,800x700");

  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  auto* audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  auto* camera_icon = GetSecondaryVideoConferenceTray()->camera_icon();

  ASSERT_TRUE(audio_icon->GetVisible());
  ASSERT_TRUE(camera_icon->GetVisible());

  state.has_media_app = false;
  state.has_camera_permission = false;
  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(GetSecondaryVideoConferenceTray()->GetVisible());
  EXPECT_FALSE(audio_icon->GetVisible());
  EXPECT_FALSE(camera_icon->GetVisible());
}

TEST_F(VideoConferenceTrayTest, CameraButtonVisibility) {
  // Camera icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_camera_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->GetVisible());

  state.has_camera_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, MicrophoneButtonVisibility) {
  // Microphone icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->GetVisible());

  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(audio_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, ScreenshareButtonVisibility) {
  auto* screen_share_icon = video_conference_tray()->screen_share_icon();

  VideoConferenceMediaState state;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(screen_share_icon->GetVisible());
  EXPECT_TRUE(screen_share_icon->show_privacy_indicator());

  state.is_capturing_screen = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(screen_share_icon->GetVisible());
  EXPECT_FALSE(screen_share_icon->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, ToggleCameraButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(camera_icon()->toggled());

  // Click the button should mute the camera.
  LeftClickOn(camera_icon());
  EXPECT_TRUE(controller()->GetCameraMuted());
  EXPECT_TRUE(camera_icon()->toggled());
  histogram_tester.ExpectBucketCount(kCameraMuteHistogramName, false, 1);

  // Toggle again, should be unmuted.
  LeftClickOn(camera_icon());
  EXPECT_FALSE(controller()->GetCameraMuted());
  EXPECT_FALSE(camera_icon()->toggled());
  histogram_tester.ExpectBucketCount(kCameraMuteHistogramName, true, 1);
}

TEST_F(VideoConferenceTrayTest, ToggleMicrophoneButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(audio_icon()->toggled());

  // Click the button should mute the microphone.
  LeftClickOn(audio_icon());
  EXPECT_TRUE(controller()->GetMicrophoneMuted());
  EXPECT_TRUE(audio_icon()->toggled());
  histogram_tester.ExpectBucketCount(kMicrophoneMuteHistogramName, false, 1);

  // Toggle again, should be unmuted.
  LeftClickOn(audio_icon());
  EXPECT_FALSE(controller()->GetMicrophoneMuted());
  EXPECT_FALSE(audio_icon()->toggled());
  histogram_tester.ExpectBucketCount(kMicrophoneMuteHistogramName, true, 1);
}

TEST_F(VideoConferenceTrayTest, ClickScreenshareButton) {
  base::HistogramTester histogram_tester;
  SetTrayAndButtonsVisible();

  EXPECT_EQ(controller()->stop_all_screen_share_count(), 0);
  // Click the screen share button should trigger the screen access stop
  // callback.
  LeftClickOn(screen_share_icon());
  histogram_tester.ExpectBucketCount(kStopScreenShareHistogramName, true, 1);

  EXPECT_EQ(controller()->stop_all_screen_share_count(), 1);
}

TEST_F(VideoConferenceTrayTest, PrivacyIndicator) {
  auto state = SetTrayAndButtonsVisible();

  // Privacy indicator should be shown when camera is actively capturing video.
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // Should not show indicator when not capture.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, CameraIconPrivacyIndicatorOnToggled) {
  auto state = SetTrayAndButtonsVisible();

  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);

  EXPECT_TRUE(camera_icon()->show_privacy_indicator());
  EXPECT_TRUE(camera_icon()->GetVisible());

  // Privacy indicator should not be shown when camera button is toggled.
  LeftClickOn(camera_icon());
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, MicrophoneIconPrivacyIndicatorOnToggled) {
  auto state = SetTrayAndButtonsVisible();
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);

  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // Privacy indicator should not be shown when audio button is toggled.
  LeftClickOn(audio_icon());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
}

// Tests that an autohidden shelf is shown after a new app begins capturing.
TEST_F(VideoConferenceTrayTest, AutoHiddenShelfShownSingleDisplay) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);

  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  SetTrayAndButtonsVisible();

  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  auto& disable_shelf_autohide_timer =
      controller()->GetShelfAutoHideTimerForTest();
  EXPECT_TRUE(disable_shelf_autohide_timer.IsRunning());

  // Fast forward so the timer expires.
  task_environment()->FastForwardBy(base::Seconds(7));

  ASSERT_FALSE(disable_shelf_autohide_timer.IsRunning());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that an autohidden shelf is re-shown after a new app begins capturing.
TEST_F(VideoConferenceTrayTest, AutoHiddenShelfReShown) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);

  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  auto state = SetTrayAndButtonsVisible();

  auto& disable_shelf_autohide_timer =
      controller()->GetShelfAutoHideTimerForTest();
  // Fast forward so the timer expires.
  task_environment()->FastForwardBy(base::Seconds(7));

  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_FALSE(disable_shelf_autohide_timer.IsRunning());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Add a second app, the shelf should re-show.
  ModifyAppsCapturing(/*add=*/true);
  controller()->UpdateWithMediaState(state);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(disable_shelf_autohide_timer.IsRunning());

  // Fast forward so the timer expires. The shelf should re hide.
  task_environment()->FastForwardBy(base::Seconds(7));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that the shelf is shown for at least 6s after the most recent app
// starts capturing.
TEST_F(VideoConferenceTrayTest, AutoHiddenShelfTimerRestarted) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);

  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  auto state = SetTrayAndButtonsVisible();

  // Fast forward for 2/3rds of the timer duration, then simulate a second app
  // capturing. The timer should extend for another 6s.
  task_environment()->FastForwardBy(base::Seconds(4));
  ModifyAppsCapturing(/*add=*/true);
  controller()->UpdateWithMediaState(state);

  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Fast forward for 2/3rds of the timer duration again. The timer should have
  // been restarted, and it should still be running.
  task_environment()->FastForwardBy(base::Seconds(4));
  auto& disable_shelf_autohide_timer =
      controller()->GetShelfAutoHideTimerForTest();
  EXPECT_TRUE(disable_shelf_autohide_timer.IsRunning());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Fast forward a final 2/3rds, the shelf should be hidden.
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(disable_shelf_autohide_timer.IsRunning());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that a decreased app count does not show the shelf.
TEST_F(VideoConferenceTrayTest, DecreasedAppCountDoesNotShowShelf) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);

  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  SetTrayAndButtonsVisible();
  // Fast forward to fire the timer and hide the shelf.
  task_environment()->FastForwardBy(base::Seconds(7));
  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Simulate a decrease in number of apps capturing, the shelf should still be
  // hidden.
  controller()->ClearMediaApps();
  controller()->UpdateWithMediaState(VideoConferenceMediaState());

  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_FALSE(controller()->GetShelfAutoHideTimerForTest().IsRunning());
}

// Tests that a decreased app count has no effect on a running timer if another
// app is capturing.
TEST_F(VideoConferenceTrayTest, DecreasedAppCountDoesNotHideShelf) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);
  ModifyAppsCapturing(/*add=*/true);
  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  auto state = SetTrayAndButtonsVisible();
  // Fast forward the timer by 1/3rd, the shelf should still be shown.
  task_environment()->FastForwardBy(base::Seconds(2));
  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Simulate a decrease in number of apps capturing, the shelf should still be
  // shown.
  ModifyAppsCapturing(/*add=*/false);
  controller()->UpdateWithMediaState(state);
  // Fast forward the timer to 2/3rds, the shelf should still be shown.
  task_environment()->FastForwardBy(base::Seconds(2));

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(controller()->GetShelfAutoHideTimerForTest().IsRunning());
}

// Tests that the shelf hides when the number of apps drops to 0.
TEST_F(VideoConferenceTrayTest, AppCountFromOneToZero) {
  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();
  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);
  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  SetTrayAndButtonsVisible();
  // Fast forward 1/3rd of the timer length, the shelf should still be shown.
  task_environment()->FastForwardBy(base::Seconds(2));
  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Simulate that no more apps are capturing. The shelf should hide.
  ModifyAppsCapturing(/*add=*/false);
  controller()->UpdateWithMediaState(VideoConferenceMediaState());

  // To prevent flakiness, wait for the async call to fetch media apps to
  // finish, and the shelf to update states.
  do {
    task_environment()->RunUntilIdle();
  } while (shelf->GetAutoHideState() != SHELF_AUTO_HIDE_HIDDEN);

  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_FALSE(controller()->GetShelfAutoHideTimerForTest().IsRunning());
}

// Tests that disconnecting a monitor while the disable shelf autohide timer is
// running does not crash.
TEST_F(VideoConferenceTrayTest, AutoHiddenShelfTwoDisplays) {
  UpdateDisplay("800x700,800x700");

  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    Shelf* shelf = root_window_controller->shelf();
    shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  }

  auto widget = ForceShelfToAutoHideOnPrimaryDisplay();

  // Create a second window on the secondary display, the shelf should hide on
  // the secondary display as well.
  auto secondary_display_window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  secondary_display_window->SetBounds(gfx::Rect(900, 0, 100, 100));

  auto* secondary_shelf =
      Shell::Get()
          ->GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  ASSERT_EQ(SHELF_AUTO_HIDE, secondary_shelf->GetVisibilityState());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, secondary_shelf->GetAutoHideState());

  // Update the list of media apps in the mock controller so the
  // VideoConferenceTray sees that a new app has begun capturing.
  ModifyAppsCapturing(/*add=*/true);

  // Update the `VideoConferenceMediaState` to force the `VideoConferenceTray`
  // to show. The shelf should also show, since the number of apps capturing has
  // increased.
  SetTrayAndButtonsVisible();

  auto* primary_shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, primary_shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, primary_shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, secondary_shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, secondary_shelf->GetAutoHideState());

  // Simulate disconnecting the second display, there should be no crash.
  UpdateDisplay("800x700");
  // Re-set the autohide behavior.
  primary_shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  auto& disable_shelf_autohide_timer =
      controller()->GetShelfAutoHideTimerForTest();
  ASSERT_TRUE(disable_shelf_autohide_timer.IsRunning());

  disable_shelf_autohide_timer.FireNow();

  EXPECT_EQ(SHELF_AUTO_HIDE, primary_shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, primary_shelf->GetAutoHideState());
}

// Tests that the `VideoConferenceTray` is visible when a display is connected
// after a session begins. All the icons should have correct states.
TEST_F(VideoConferenceTrayTest, MultiDisplayVideoConferenceTrayVisibility) {
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_microphone = true;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);

  ASSERT_TRUE(video_conference_tray()->GetVisible());

  // Mute the camera by clicking on the icon.
  LeftClickOn(camera_icon());
  ASSERT_TRUE(camera_icon()->toggled());

  // Attach a second display, the VideoConferenceTray on the second display
  // should be visible.
  UpdateDisplay("800x700,800x700");

  EXPECT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // All the icons should have correct states.
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  EXPECT_TRUE(secondary_camera_icon);
  EXPECT_FALSE(secondary_camera_icon->is_capturing());
  EXPECT_TRUE(secondary_camera_icon->toggled());

  auto* secondary_microphone_icon =
      GetSecondaryVideoConferenceTray()->audio_icon();
  EXPECT_TRUE(secondary_microphone_icon);
  EXPECT_TRUE(secondary_microphone_icon->is_capturing());
  EXPECT_FALSE(secondary_microphone_icon->toggled());

  auto* secondary_screen_share_icon =
      GetSecondaryVideoConferenceTray()->screen_share_icon();
  EXPECT_TRUE(secondary_screen_share_icon);
  EXPECT_TRUE(secondary_screen_share_icon->is_capturing());
  EXPECT_FALSE(secondary_screen_share_icon->toggled());
}

// Tests that privacy indicators update on secondary displays when a capture
// session begins.
TEST_F(VideoConferenceTrayTest, PrivacyIndicatorOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  EXPECT_TRUE(secondary_camera_icon->GetVisible());
  EXPECT_TRUE(secondary_camera_icon->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(secondary_audio_icon->show_privacy_indicator());

  // Should not show indicator when not capturing.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);

  EXPECT_FALSE(secondary_camera_icon->show_privacy_indicator());
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());
}

// Tests that the camera toggle state updates across displays.
TEST_F(VideoConferenceTrayTest, CameraButtonToggleAcrossDisplays) {
  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Mute the camera on the primary display.
  LeftClickOn(camera_icon());
  ASSERT_TRUE(controller()->GetCameraMuted());
  ASSERT_TRUE(camera_icon()->toggled());

  // The secondary display camera icon should be toggled.
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  EXPECT_TRUE(secondary_camera_icon->toggled());

  // Unmute the camera on the secondary display.
  LeftClickOn(secondary_camera_icon);

  // The secondary display camera icon should not be toggled.
  EXPECT_FALSE(secondary_camera_icon->toggled());

  // The primary display camera icon should also not be toggled and the camera
  // should not be muted.
  EXPECT_FALSE(controller()->GetCameraMuted());
  EXPECT_FALSE(camera_icon()->toggled());
}

// Tests that the audio toggle state updates across displays.
TEST_F(VideoConferenceTrayTest, AudioButtonToggleAcrossDisplays) {
  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Mute the audio on the primary display.
  LeftClickOn(audio_icon());
  ASSERT_TRUE(controller()->GetMicrophoneMuted());
  ASSERT_TRUE(audio_icon()->toggled());

  // The secondary display audio icon should be toggled.
  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();
  EXPECT_TRUE(secondary_audio_icon->toggled());

  // Unmute the audio on the secondary display.
  LeftClickOn(secondary_audio_icon);

  // The secondary display audio icon should not be toggled.
  EXPECT_FALSE(secondary_audio_icon->toggled());

  // The primary display audio icon should also not be toggled and the audio
  // should not be muted.
  EXPECT_FALSE(controller()->GetMicrophoneMuted());
  EXPECT_FALSE(audio_icon()->toggled());
}

// Tests that the camera privacy indicators update on toggle across displays.
TEST_F(VideoConferenceTrayTest,
       PrivacyIndicatorToggleCameraOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Turn privacy indicators on for the camera.
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);

  // Toggle the camera off on the primary, the indicator should be updated on
  // the secondary.
  auto* secondary_camera_icon =
      GetSecondaryVideoConferenceTray()->camera_icon();
  LeftClickOn(camera_icon());
  ASSERT_FALSE(camera_icon()->show_privacy_indicator());
  EXPECT_FALSE(secondary_camera_icon->show_privacy_indicator());

  // Toggle the camera back on on the secondary, the indicator should be updated
  // on the primary.
  LeftClickOn(secondary_camera_icon);
  ASSERT_TRUE(secondary_camera_icon->show_privacy_indicator());
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());
}

// Tests that the microphone privacy indicators update on toggle across
// displays.
TEST_F(VideoConferenceTrayTest, PrivacyIndicatorToggleAudioOnSecondaryDisplay) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(GetSecondaryVideoConferenceTray()->GetVisible());

  // Turn privacy indicators on for the microphone.
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);

  auto* secondary_audio_icon = GetSecondaryVideoConferenceTray()->audio_icon();

  // Toggle the audio off on the primary, the indicator should be updated on the
  // secondary.
  LeftClickOn(audio_icon());
  ASSERT_FALSE(audio_icon()->show_privacy_indicator());
  EXPECT_FALSE(secondary_audio_icon->show_privacy_indicator());

  // Toggle the audio back on on the secondary, the indicator should be updated
  // on the primary.
  LeftClickOn(secondary_audio_icon);
  ASSERT_TRUE(secondary_audio_icon->show_privacy_indicator());
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());
}

// Tests that the tray is visible only in an active session.
TEST_F(VideoConferenceTrayTest, SessionChanged) {
  SetTrayAndButtonsVisible();

  SetSessionState(session_manager::SessionState::OOBE);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(video_conference_tray()->GetVisible());

  // Locks screen. The tray should be hidden.
  SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(video_conference_tray()->GetVisible());

  // Switches back to active. The tray should show.
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(video_conference_tray()->GetVisible());
}

// Test that updating the state of a toggle updates the tooltip.
TEST_F(VideoConferenceTrayTest, MutingChangesTooltip) {
  auto state = SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());

  // The button is not toggled by default, and should not be capturing.
  ASSERT_FALSE(audio_icon()->toggled());

  EXPECT_EQ(
      audio_icon()->GetTooltipText(),
      l10n_util::GetStringFUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
          l10n_util::GetStringUTF16(
              VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE),
          l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON)));

  // Update the state to capturing, the tooltip should update.
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);

  EXPECT_EQ(audio_icon()->GetTooltipText(),
            l10n_util::GetStringFUTF16(
                VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
                l10n_util::GetStringUTF16(
                    VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE),
                l10n_util::GetStringUTF16(
                    VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON_AND_IN_USE)));

  // Toggle the audio off, the tooltip should be updated.
  LeftClickOn(audio_icon());
  ASSERT_TRUE(controller()->GetMicrophoneMuted());
  ASSERT_TRUE(audio_icon()->toggled());

  EXPECT_EQ(
      audio_icon()->GetTooltipText(),
      l10n_util::GetStringFUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
          l10n_util::GetStringUTF16(
              VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE),
          l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
}

TEST_F(VideoConferenceTrayTest, CloseBubbleOnEffectSupportStateChange) {
  SetTrayAndButtonsVisible();

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  ASSERT_TRUE(video_conference_tray()->GetBubbleView());

  controller()->GetEffectsManager().NotifyEffectSupportStateChanged(
      VcEffectId::kTestEffect, /*is_supported=*/true);

  // When there's a change to effect support state, the bubble should be
  // automatically close to update.
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
}

TEST_F(VideoConferenceTrayTest, BubbleWithOnlyLinuxApps) {
  SetTrayAndButtonsVisible();

  // Create 1 non-linux app. We should show `kMainBubbleView`.
  CreateMediaApps(1, /*clear_existing_apps=*/true);
  LeftClickOn(toggle_bubble_button());
  auto* bubble_view = video_conference_tray()->GetBubbleView();
  ASSERT_TRUE(bubble_view);

  EXPECT_EQ(video_conference::BubbleViewID::kMainBubbleView,
            bubble_view->GetID());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Create 1 linux app. We should show `kLinuxAppBubbleView`.
  CreateMediaApps(1, /*clear_existing_apps=*/true,
                  crosapi::mojom::VideoConferenceAppType::kBorealis);
  LeftClickOn(toggle_bubble_button());
  bubble_view = video_conference_tray()->GetBubbleView();
  ASSERT_TRUE(bubble_view);

  EXPECT_EQ(video_conference::BubbleViewID::kLinuxAppBubbleView,
            bubble_view->GetID());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Create 1 linux app and 1 non-linux app. We should still show
  // `kMainBubbleView`.
  CreateMediaApps(1, /*clear_existing_apps=*/true,
                  crosapi::mojom::VideoConferenceAppType::kBorealis);
  CreateMediaApps(1, /*clear_existing_apps=*/false);
  LeftClickOn(toggle_bubble_button());
  bubble_view = video_conference_tray()->GetBubbleView();
  ASSERT_TRUE(bubble_view);

  EXPECT_EQ(video_conference::BubbleViewID::kMainBubbleView,
            bubble_view->GetID());
}

// Tests the tray when there's a delay in `GetMediaApps()`.
class VideoConferenceTrayDelayTest : public VideoConferenceTrayTest {
 public:
  VideoConferenceTrayDelayTest() = default;
  VideoConferenceTrayDelayTest(const VideoConferenceTrayDelayTest&) = delete;
  VideoConferenceTrayDelayTest& operator=(const VideoConferenceTrayDelayTest&) =
      delete;
  ~VideoConferenceTrayDelayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    delay_controller_ = std::make_unique<DelayVideoConferenceTrayController>();

    AshTestBase::SetUp();
  }

  DelayVideoConferenceTrayController* delay_controller() {
    return delay_controller_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DelayVideoConferenceTrayController> delay_controller_;
};

TEST_F(VideoConferenceTrayDelayTest, OpenBubble) {
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_screen = true;
  delay_controller()->UpdateWithMediaState(state);

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());

  // First it should not be visible since `GetMediaApps()` is running.
  ASSERT_FALSE(video_conference_tray()->GetBubbleView());

  // Bubble should appear after the delay.
  task_environment()->FastForwardBy(kGetMediaAppsDelayTime);

  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_EQ(1, delay_controller()->getting_media_apps_called());

  LeftClickOn(toggle_bubble_button());
  ASSERT_FALSE(video_conference_tray()->GetBubbleView());

  // Spam clicking the button should only cost one extra call to
  // `GetMediaApps()`.
  LeftClickOn(toggle_bubble_button());
  LeftClickOn(toggle_bubble_button());
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(2, delay_controller()->getting_media_apps_called());

  // Bubble should appear after the delay.
  task_environment()->FastForwardBy(kGetMediaAppsDelayTime);
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
}

}  // namespace ash
