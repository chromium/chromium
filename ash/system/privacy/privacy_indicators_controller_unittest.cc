// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

class TestDelegate : public PrivacyIndicatorsNotificationDelegate {
 public:
  explicit TestDelegate(bool has_launch_app_callback = true,
                        bool has_launch_settings_callback = true) {
    if (has_launch_app_callback) {
      SetLaunchAppCallback(base::BindRepeating(
          &TestDelegate::LaunchApp, weak_pointer_factory_.GetWeakPtr()));
    }
    if (has_launch_settings_callback) {
      SetLaunchSettingsCallback(
          base::BindRepeating(&TestDelegate::LaunchAppSettings,
                              weak_pointer_factory_.GetWeakPtr()));
    }
  }

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  void LaunchApp() { launch_app_called_ = true; }
  void LaunchAppSettings() { launch_settings_called_ = true; }

  bool launch_app_called() { return launch_app_called_; }
  bool launch_settings_called() { return launch_settings_called_; }

 private:
  ~TestDelegate() override = default;

  bool launch_app_called_ = false;
  bool launch_settings_called_ = false;

  base::WeakPtrFactory<TestDelegate> weak_pointer_factory_{this};
};

void ExpectPrivacyIndicatorsTrayItemVisible(bool visible,
                                            bool camera_visible,
                                            bool microphone_visible) {
  for (auto* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    auto* privacy_indicators_view =
        root_window_controller->GetStatusAreaWidget()
            ->unified_system_tray()
            ->privacy_indicators_view();

    ASSERT_TRUE(privacy_indicators_view);
    EXPECT_EQ(visible, privacy_indicators_view->GetVisible());

    if (visible) {
      EXPECT_EQ(camera_visible,
                privacy_indicators_view->camera_icon()->GetVisible());
      EXPECT_EQ(microphone_visible,
                privacy_indicators_view->microphone_icon()->GetVisible());
    }
  }
}

}  // namespace

class PrivacyIndicatorsControllerTest : public AshTestBase {
 public:
  PrivacyIndicatorsControllerTest()
      : scoped_feature_list_(features::kPrivacyIndicators) {}
  PrivacyIndicatorsControllerTest(const PrivacyIndicatorsControllerTest&) =
      delete;
  PrivacyIndicatorsControllerTest& operator=(
      const PrivacyIndicatorsControllerTest&) = delete;
  ~PrivacyIndicatorsControllerTest() override = default;

  // Get the notification view from message center associated with `id`.
  message_center::NotificationViewBase* GetNotificationViewFromMessageCenter(
      const std::string& id) {
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    auto* view = GetPrimaryUnifiedSystemTray()
                     ->message_center_bubble()
                     ->notification_center_view()
                     ->notification_list_view()
                     ->GetMessageViewForNotificationId(id);
    auto* notification_view =
        static_cast<message_center::NotificationViewBase*>(view);
    EXPECT_TRUE(notification_view);
    return notification_view;
  }

  // Get the popup notification view associated with `id`.
  views::View* GetPopupNotificationView(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->GetMessagePopupCollection()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickView(message_center::NotificationViewBase* view, int button_index) {
    auto* action_buttons = view->GetViewByID(
        message_center::NotificationViewBase::kActionButtonsRow);

    auto* button_view = action_buttons->children()[button_index];

    ui::test::EventGenerator generator(GetRootWindow(button_view->GetWidget()));
    gfx::Point cursor_location = button_view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }

  PrivacyIndicatorsTrayItemView* GetPrimaryDisplayPrivacyIndicatorsView()
      const {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->unified_system_tray()
        ->privacy_indicators_view();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyIndicatorsControllerTest, NotificationMetadata) {
  std::string app_id = "test_app_id";
  std::u16string app_name = u"test_app_name";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, app_name, /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // Notification message should contains app name.
  EXPECT_NE(std::string::npos, notification->message().find(app_name));

  // Privacy indicators notification should not be a popup. It is silently added
  // to the tray.
  EXPECT_FALSE(GetPopupNotificationView(notification_id));
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationWithNoButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/false,
      /*has_launch_settings_callback=*/false);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // With the delegate don't provide any callbacks, the notification
  // should have one button for launching the app.
  EXPECT_EQ(0u, notification->buttons().size());
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationClickWithLaunchAppButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/true, /*has_launch_settings_callback=*/false);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides only launch app callbacks, the notification
  // should have one button for launching the app.
  auto buttons = notification->buttons();
  ASSERT_EQ(1u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_LAUNCH),
      buttons[0].title);

  // Clicking that button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_app_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_app_called());
}

TEST_F(PrivacyIndicatorsControllerTest,
       NotificationClickWithLaunchSettingsButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/false, /*has_launch_settings_callback=*/true);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides only launch settings callbacks, the notification
  // should have one button for launching the app settings.
  auto buttons = notification->buttons();
  ASSERT_EQ(1u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS),
      buttons[0].title);

  // Clicking that button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_settings_called());
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationClickWithTwoButtons) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides both launch app and launch settings callbacks,
  // the notification should have 2 buttons. The first one is the launch app and
  // the second one is the launch button.
  auto buttons = notification->buttons();
  ASSERT_EQ(2u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_LAUNCH),
      buttons[0].title);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS),
      buttons[1].title);

  // Clicking the first button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_app_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_app_called());

  // Clicking the second button will trigger launching the app settings.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickView(notification_view, 1);
  EXPECT_TRUE(delegate->launch_settings_called());
}

// Tests that privacy indicators notifications are working properly when there
// are two running apps.
TEST_F(PrivacyIndicatorsControllerTest, NotificationWithTwoApps) {
  std::string app_id1 = "test_app_id1";
  std::string app_id2 = "test_app_id2";
  std::string notification_id1 = GetPrivacyIndicatorsNotificationId(app_id1);
  std::string notification_id2 = GetPrivacyIndicatorsNotificationId(app_id2);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id1, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id2, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* message_center = message_center::MessageCenter::Get();
  EXPECT_TRUE(message_center->FindNotificationById(notification_id1));
  EXPECT_TRUE(message_center->FindNotificationById(notification_id2));

  // A group parent notification should also be created for these 2
  // notifications.
  std::string id_parent =
      notification_id1 + message_center::kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(id_parent));

  // Update the state. All notifications should be removed.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id1, u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id2, u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);

  EXPECT_FALSE(message_center->FindNotificationById(notification_id1));
  EXPECT_FALSE(message_center->FindNotificationById(notification_id2));
  EXPECT_FALSE(message_center->FindNotificationById(id_parent));
}

// Tests that a basic privacy indicator notification is disabled when the video
// conference feature is enabled.
TEST_F(PrivacyIndicatorsControllerTest,
       DoNotShowNotificationWithVideoConferenceEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kVideoConference};
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kCameraEffectsSupportedByHardware);
  // Try to show a notification.
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  // The notification should not exist.
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
}

// Tests privacy indicators tray item visibility across all status area widgets.
TEST_F(PrivacyIndicatorsControllerTest, PrivacyIndicatorsTrayItemView) {
  // Uses normal animation duration so that the icons would not be immediately
  // hidden after the animation.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Make sure privacy indicators work on multiple displays.
  UpdateDisplay("300x200,500x400");

  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  ExpectPrivacyIndicatorsTrayItemVisible(
      /*visible=*/true, /*camera_visible=*/true, /*microphone_visible=*/false);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);
  ExpectPrivacyIndicatorsTrayItemVisible(
      /*visible=*/true, /*camera_visible=*/true, /*microphone_visible=*/true);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  ExpectPrivacyIndicatorsTrayItemVisible(
      /*visible=*/false, /*camera_visible=*/false,
      /*microphone_visible=*/false);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);
  ExpectPrivacyIndicatorsTrayItemVisible(
      /*visible=*/true, /*camera_visible=*/false, /*microphone_visible=*/true);
}

TEST_F(PrivacyIndicatorsControllerTest, SourceMetricsCollection) {
  base::HistogramTester histogram_tester;
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  const std::string histogram_name = "Ash.PrivacyIndicators.Source";

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     PrivacyIndicatorsSource::kApps, 1);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate,
      PrivacyIndicatorsSource::kScreenCapture);
  histogram_tester.ExpectBucketCount(
      histogram_name, PrivacyIndicatorsSource::kScreenCapture, 1);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      "test_id", u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate,
      PrivacyIndicatorsSource::kLinuxVm);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     PrivacyIndicatorsSource::kLinuxVm, 1);
}

TEST_F(PrivacyIndicatorsControllerTest, CameraDisabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  ASSERT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Camera is in use, but if the camera is being hardware muted, the
  // notification and indicator view should not show.
  controller->OnCameraHWPrivacySwitchStateChanged(
      "test_device_id", cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Flip back the switch. Indicators should show again.
  controller->OnCameraHWPrivacySwitchStateChanged(
      "test_device_id", cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Camera is in use, but if the camera is being software muted, the
  // notification and indicator view should not show.
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());
  // Flip back the switch. Indicators should show again.
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // If both camera and microphone is in use, but the camera is muted. The
  // notification content (i.e. the title) should reflect that only mic is being
  // in used.
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC),
            notification->title());

  // Flip back.
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  notification = message_center::MessageCenter::Get()->FindNotificationById(
      notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC),
      notification->title());
}

TEST_F(PrivacyIndicatorsControllerTest, MicrophoneDisabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  ASSERT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Microphone is in use, but if the microphone is being muted, the
  // notification and indicator view should not show.
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Flip back the switch. Indicators should show again.
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/false,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // If both camera and microphone is in use, but the camera is muted. The
  // notification content (i.e. the title) should reflect that only mic is being
  // in used.
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
            notification->title());

  // Flip back.
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/false,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  notification = message_center::MessageCenter::Get()->FindNotificationById(
      notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC),
      notification->title());
}

// When both microphone and camera is disabled, no privacy indicators should
// show for camera/microphone usage.
TEST_F(PrivacyIndicatorsControllerTest, CameraAndMicrophoneDisabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  ASSERT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);

  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  controller->OnCameraHWPrivacySwitchStateChanged(
      "test_device_id", cros::mojom::CameraPrivacySwitchState::ON);

  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());
}

TEST_F(PrivacyIndicatorsControllerTest, CameraDisabledWithMultipleApps) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id1 = "test_app_id1";
  std::string app_id2 = "test_app_id2";
  std::string app_id3 = "test_app_id3";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id1, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->UpdatePrivacyIndicators(app_id2, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->UpdatePrivacyIndicators(app_id3, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id1 = GetPrivacyIndicatorsNotificationId(app_id1);
  std::string notification_id2 = GetPrivacyIndicatorsNotificationId(app_id2);
  std::string notification_id3 = GetPrivacyIndicatorsNotificationId(app_id3);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id3));

  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));

  // The app that uses microphone should not be affected.
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id3));

  // When flip back, all old notification should be re-created.
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);

  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));
}

TEST_F(PrivacyIndicatorsControllerTest, MicrophoneDisabledWithMultipleApps) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id1 = "test_app_id1";
  std::string app_id2 = "test_app_id2";
  std::string app_id3 = "test_app_id3";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id1, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->UpdatePrivacyIndicators(app_id2, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->UpdatePrivacyIndicators(app_id3, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id1 = GetPrivacyIndicatorsNotificationId(app_id1);
  std::string notification_id2 = GetPrivacyIndicatorsNotificationId(app_id2);
  std::string notification_id3 = GetPrivacyIndicatorsNotificationId(app_id3);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id3));

  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/true,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));

  // The app that uses camera should not be affected.
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id3));

  // When flip back, all old notification should be re-created.
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/false,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);

  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id2));
}

// Tests enabling both `kPrivacyIndicators` and `kVideoConference`,
// parameterized with `kQsRevamp` enabled and disabled.
class PrivacyIndicatorsControllerVideoConferenceTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyIndicatorsControllerVideoConferenceTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kPrivacyIndicators, features::kVideoConference};
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsQsRevampEnabled()) {
      enabled_features.push_back(features::kQsRevamp);
    } else {
      disabled_features.push_back(features::kQsRevamp);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  PrivacyIndicatorsControllerVideoConferenceTest(
      const PrivacyIndicatorsControllerVideoConferenceTest&) = delete;
  PrivacyIndicatorsControllerVideoConferenceTest& operator=(
      const PrivacyIndicatorsControllerVideoConferenceTest&) = delete;
  ~PrivacyIndicatorsControllerVideoConferenceTest() override = default;

  bool IsQsRevampEnabled() { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyIndicatorsControllerVideoConferenceTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

// Make sure that when `kPrivacyIndicators` and `kVideoConference` are both
// enabled, the privacy indicators view and the controller is not created.
TEST_P(PrivacyIndicatorsControllerVideoConferenceTest, ObjectsCreation) {
  EXPECT_FALSE(PrivacyIndicatorsController::Get());

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
    DCHECK(status_area_widget);

    auto* privacy_indicators_view =
        features::IsQsRevampEnabled()
            ? status_area_widget->notification_center_tray()
                  ->privacy_indicators_view()
            : status_area_widget->unified_system_tray()
                  ->privacy_indicators_view();

    EXPECT_FALSE(privacy_indicators_view);
  }
}

}  // namespace ash
