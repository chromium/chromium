// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/message_view_container.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

void SetSessionState(session_manager::SessionState state) {
  ash::SessionInfo info;
  info.state = state;
  ash::Shell::Get()->session_controller()->SetSessionInfo(info);
}

class TestDelegate : public PrivacyIndicatorsNotificationDelegate {
 public:
  explicit TestDelegate(bool has_launch_settings_callback = true) {
    if (has_launch_settings_callback) {
      SetLaunchSettingsCallback(
          base::BindRepeating(&TestDelegate::LaunchAppSettings,
                              weak_pointer_factory_.GetWeakPtr()));
    }
  }

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  void LaunchAppSettings() { launch_settings_called_ = true; }

  bool launch_settings_called() { return launch_settings_called_; }

 private:
  ~TestDelegate() override = default;

  bool launch_settings_called_ = false;

  base::WeakPtrFactory<TestDelegate> weak_pointer_factory_{this};
};

void ExpectPrivacyIndicatorsTrayItemVisible(bool visible,
                                            bool camera_visible,
                                            bool microphone_visible) {
  for (auto* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    StatusAreaWidget* status_area_widget =
        root_window_controller->GetStatusAreaWidget();
    PrivacyIndicatorsTrayItemView* privacy_indicators_view =
        status_area_widget->notification_center_tray()
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

class PrivacyIndicatorsControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyIndicatorsControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureState(features::kOngoingProcesses,
                                              AreOngoingProcessesEnabled());
  }

  // Get the notification view from message center associated with `id`.
  const message_center::MessageView* GetMessageViewFromMessageCenter(
      const std::string& id) {
    NotificationCenterTray* notification_center_tray =
        Shell::GetPrimaryRootWindowController()
            ->GetStatusAreaWidget()
            ->notification_center_tray();
    notification_center_tray->ShowBubble();

    if (features::AreOngoingProcessesEnabled()) {
      return notification_center_tray->bubble()
          ->GetOngoingProcessMessageViewContainerById(id)
          ->message_view();
    }

    return notification_center_tray->GetNotificationListView()
        ->GetMessageViewForNotificationId(id);
  }

  // Get the popup notification view associated with `id`.
  views::View* GetPopupNotificationView(const std::string& id) {
    return GetPrimaryNotificationCenterTray()
        ->popup_collection()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickPrimaryNotificationButton(const std::string& id) {
    if (features::AreOngoingProcessesEnabled()) {
      auto* notification_view = GetMessageViewFromMessageCenter(id);
      ASSERT_TRUE(notification_view);
      auto* button_view = notification_view->GetViewByID(
          VIEW_ID_ONGOING_PROCESS_PRIMARY_ICON_BUTTON);
      ASSERT_TRUE(button_view);
      LeftClickOn(button_view);
      return;
    }

    auto* notification_view = GetMessageViewFromMessageCenter(id);
    ASSERT_TRUE(notification_view);
    auto* action_buttons = notification_view->GetViewByID(
        message_center::NotificationViewBase::kActionButtonsRow);
    ASSERT_TRUE(action_buttons);
    auto* button_view = action_buttons->children()[0].get();
    ASSERT_TRUE(button_view);
    LeftClickOn(button_view);
  }

  PrivacyIndicatorsTrayItemView* GetPrimaryDisplayPrivacyIndicatorsView()
      const {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->privacy_indicators_view();
  }

  bool AreOngoingProcessesEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyIndicatorsControllerTest,
                         /*are_ongoing_processes_enabled=*/testing::Bool());

TEST_P(PrivacyIndicatorsControllerTest, NotificationMetadata) {
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
  EXPECT_TRUE(base::Contains((notification->title()), app_name));

  // Privacy indicators notification should not be a popup. It is silently added
  // to the tray.
  EXPECT_FALSE(GetPopupNotificationView(notification_id));
}

TEST_P(PrivacyIndicatorsControllerTest, NotificationWithNoButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
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

TEST_P(PrivacyIndicatorsControllerTest,
       NotificationClickWithLaunchSettingsButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_settings_callback=*/true);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // With the delegate provides only launch settings callbacks, the notification
  // should have one button for launching the app settings.
  auto buttons = notification->buttons();
  ASSERT_EQ(1u, buttons.size());

  // Clicking that button will trigger launching the app settings.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickPrimaryNotificationButton(notification_id);
  EXPECT_TRUE(delegate->launch_settings_called());
}

TEST_P(PrivacyIndicatorsControllerTest, NotificationClickBody) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);

  // Create a notification without a launch settings callback.
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_settings_callback=*/false);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate, PrivacyIndicatorsSource::kApps);

  auto* notification_view = GetMessageViewFromMessageCenter(notification_id);
  ASSERT_TRUE(notification_view);

  // Clicking the notification body without a launch settings callback will not
  // do anything.
  EXPECT_FALSE(delegate->launch_settings_called());
  LeftClickOn(notification_view);
  EXPECT_FALSE(delegate->launch_settings_called());

  // Update the notification so it has a launch settings callback.
  scoped_refptr<TestDelegate> delegate_with_settings_callback =
      base::MakeRefCounted<TestDelegate>(
          /*has_launch_settings_callback=*/true);
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, u"test_app_name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate_with_settings_callback,
      PrivacyIndicatorsSource::kApps);

  // Clicking the notification body with a launch settings callback should
  // launch the app settings.
  EXPECT_FALSE(delegate_with_settings_callback->launch_settings_called());
  LeftClickOn(notification_view);
  EXPECT_TRUE(delegate_with_settings_callback->launch_settings_called());
}

// Tests that privacy indicators notifications are working properly when there
// are two running apps.
TEST_P(PrivacyIndicatorsControllerTest, NotificationWithTwoApps) {
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
      notification_id1 +
      message_center_utils::GenerateGroupParentNotificationIdSuffix(
          message_center->FindNotificationById(notification_id1)
              ->notifier_id());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent));

  // Update the state. All notifications should be removed.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id1, u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id2, u"test_app_name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);

  EXPECT_FALSE(message_center->FindNotificationById(notification_id1));
  EXPECT_FALSE(message_center->FindNotificationById(notification_id2));
  EXPECT_FALSE(message_center->FindNotificationById(id_parent));
}

// Tests privacy indicators tray item visibility across all status area widgets.
TEST_P(PrivacyIndicatorsControllerTest, PrivacyIndicatorsTrayItemView) {
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
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
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

TEST_P(PrivacyIndicatorsControllerTest, SourceMetricsCollection) {
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

TEST_P(PrivacyIndicatorsControllerTest, CameraDisabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  std::u16string app_name = u"test_app_name";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, app_name,
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
  controller->UpdatePrivacyIndicators(app_id, app_name,
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC, app_name),
      notification->title());

  // Flip back.
  controller->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);
  notification = message_center::MessageCenter::Get()->FindNotificationById(
      notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC, app_name),
            notification->title());
}

TEST_P(PrivacyIndicatorsControllerTest, MicrophoneDisabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  std::u16string app_name = u"test_app_name";
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
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
                                       app_name),
            notification->title());

  // Flip back.
  CrasAudioHandler::Get()->SetInputMute(
      /*mute_on=*/false,
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
  notification = message_center::MessageCenter::Get()->FindNotificationById(
      notification_id);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC, app_name),
            notification->title());
}

// When both microphone and camera is disabled, no privacy indicators should
// show for camera/microphone usage.
TEST_P(PrivacyIndicatorsControllerTest, CameraAndMicrophoneDisabledWithOneApp) {
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

TEST_P(PrivacyIndicatorsControllerTest, CameraDisabledWithMultipleApps) {
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

TEST_P(PrivacyIndicatorsControllerTest, MicrophoneDisabledWithMultipleApps) {
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

TEST_P(PrivacyIndicatorsControllerTest,
       HidingDelayTimerMinimumEnabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);

  std::string notification_id1 = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));

  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());
}

TEST_P(PrivacyIndicatorsControllerTest, HidingDelayTimerHoldEnabledWithOneApp) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);

  std::string notification_id1 = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));

  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Fast forward by the after use duration the privacy indicator should be
  // held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsHoldAfterUseDuration);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id1));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());
}

// Tests to make sure that privacy indicators are updated accordingly in locked
// screen.
TEST_P(PrivacyIndicatorsControllerTest, UpdateUsageStageInLockScreen) {
  auto* controller = PrivacyIndicatorsController::Get();

  std::string app_id = "test_app_id";
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();

  SetSessionState(session_manager::SessionState::ACTIVE);

  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/true,
                                      /*is_microphone_used=*/true, delegate,
                                      PrivacyIndicatorsSource::kApps);

  // Privacy indicators should show up as expected in an active session.
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  ASSERT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  ASSERT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Privacy indicators should show up as expected in a locked session.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_TRUE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Update privacy indicators in a locked session. Should update accordingly.
  controller->UpdatePrivacyIndicators(app_id, u"test_app_name",
                                      /*is_camera_used=*/false,
                                      /*is_microphone_used=*/false, delegate,
                                      PrivacyIndicatorsSource::kApps);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());

  // Indicators should still show up correctly when log back to an active
  // session.
  SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
  EXPECT_FALSE(GetPrimaryDisplayPrivacyIndicatorsView()->GetVisible());
}

// Tests enabling `kVideoConference`.
class PrivacyIndicatorsControllerVideoConferenceTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyIndicatorsControllerVideoConferenceTest()
      : scoped_feature_list_(features::kFeatureManagementVideoConference) {}
  PrivacyIndicatorsControllerVideoConferenceTest(
      const PrivacyIndicatorsControllerVideoConferenceTest&) = delete;
  PrivacyIndicatorsControllerVideoConferenceTest& operator=(
      const PrivacyIndicatorsControllerVideoConferenceTest&) = delete;
  ~PrivacyIndicatorsControllerVideoConferenceTest() override = default;

  // AshTestBase:
  void SetUp() override {
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

// Make sure that when `kVideoConference` is enabled, the privacy indicators
// view and the controller is not created.
TEST_F(PrivacyIndicatorsControllerVideoConferenceTest, ObjectsCreation) {
  EXPECT_FALSE(PrivacyIndicatorsController::Get());

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
    DCHECK(status_area_widget);

    auto* privacy_indicators_view =
        status_area_widget->notification_center_tray()
            ->privacy_indicators_view();

    EXPECT_FALSE(privacy_indicators_view);
  }
}

}  // namespace ash
