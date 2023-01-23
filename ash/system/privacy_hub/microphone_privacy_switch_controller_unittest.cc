// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

using testing::_;

namespace {

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    if (sensor == Sensor::kMicrophone) {
      return apps_accessing_microphone_;
    }
    return {};
  }

  void LaunchAppAccessingMicrophone(
      const absl::optional<std::u16string> app_name) {
    if (app_name.has_value()) {
      apps_accessing_microphone_.insert(apps_accessing_microphone_.begin(),
                                        app_name.value());
    }
  }

  void CloseAppAccessingMicrophone(const std::u16string& app_name) {
    auto it = std::find(apps_accessing_microphone_.begin(),
                        apps_accessing_microphone_.end(), app_name);
    if (it != apps_accessing_microphone_.end()) {
      apps_accessing_microphone_.erase(it);
    }
  }

 private:
  std::vector<std::u16string> apps_accessing_microphone_;
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class MockFrontendAPI : public PrivacyHubDelegate {
 public:
  MOCK_METHOD(void, AvailabilityOfMicrophoneChanged, (bool), (override));
  MOCK_METHOD(void, MicrophoneHardwareToggleChanged, (bool), (override));
  void CameraHardwareToggleChanged(
      cros::mojom::CameraPrivacySwitchState state) override {}
};

}  // namespace

class PrivacyHubMicrophoneControllerTest : public AshTestBase {
 public:
  PrivacyHubMicrophoneControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);

    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    window_delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
  }
  ~PrivacyHubMicrophoneControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // This makes sure a global instance of SensorDisabledNotificationDelegate
    // is created before running tests.
    Shell::Get()->privacy_hub_controller()->set_frontend(&mock_frontend_);
  }

  void TearDown() override {
    SetMicrophoneMuteSwitchState(/*muted=*/false);
    AshTestBase::TearDown();
  }

 protected:
  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserMicrophoneAllowed, allowed);
  }

  bool GetUserPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserMicrophoneAllowed);
  }

  message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        MicrophonePrivacySwitchController::kNotificationId);
  }

  message_center::Notification* GetPopupNotification() {
    return message_center::MessageCenter::Get()->FindPopupNotificationById(
        MicrophonePrivacySwitchController::kNotificationId);
  }

  void MarkPopupAsShown() {
    message_center::MessageCenter::Get()->MarkSinglePopupAsShown(
        MicrophonePrivacySwitchController::kNotificationId, true);
  }

  void ClickOnNotificationButton() {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        MicrophonePrivacySwitchController::kNotificationId,
        /*button_index=*/0);
  }

  void ClickOnNotificationBody() {
    message_center::MessageCenter::Get()->ClickOnNotification(
        MicrophonePrivacySwitchController::kNotificationId);
  }

  void SetMicrophoneMuteSwitchState(bool muted) {
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(muted);
  }

  void MuteMicrophone() {
    CrasAudioHandler::Get()->SetInputMute(
        true, CrasAudioHandler::InputMuteChangeMethod::kOther);
  }

  void UnMuteMicrophone() {
    CrasAudioHandler::Get()->SetInputMute(
        false, CrasAudioHandler::InputMuteChangeMethod::kOther);
  }

  void SetNumberOfActiveInputStreams(int number_of_active_input_streams) {
    FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
        {{"CRAS_CLIENT_TYPE_CHROME", number_of_active_input_streams}});
  }

  void WaitUntilNotificationRemoved() {
    task_environment()->FastForwardBy(PrivacyHubNotification::kMinShowTime);
  }

  void LaunchApp(absl::optional<std::u16string> app_name) {
    delegate_.LaunchAppAccessingMicrophone(app_name);
  }

  void CloseApp(const std::u16string& app_name) {
    delegate_.CloseAppAccessingMicrophone(app_name);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }

  ::testing::NiceMock<MockFrontendAPI> mock_frontend_;

 private:
  const base::HistogramTester histogram_tester_;
  MockNewWindowDelegate* new_window_delegate_ = nullptr;
  std::unique_ptr<TestNewWindowDelegateProvider> window_delegate_provider_;
  FakeSensorDisabledNotificationDelegate delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyHubMicrophoneControllerTest, SetSystemMuteOnLogin) {
  for (bool microphone_allowed : {false, true, false}) {
    const bool microphone_muted = !microphone_allowed;
    SetUserPref(microphone_allowed);
    ASSERT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_muted);
    const AccountId user1_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();

    SimulateUserLogin("other@user.test");
    SetUserPref(microphone_muted);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_allowed);

    SimulateUserLogin(user1_account_id);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_muted);
  }
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnPreferenceChanged) {
  for (bool microphone_allowed : {false, true, false}) {
    SetUserPref(microphone_allowed);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), !microphone_allowed);
  }
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnInputMuteChanged) {
  for (bool microphone_muted : {false, true, false}) {
    const bool microphone_allowed = !microphone_muted;

    CrasAudioHandler::Get()->SetInputMute(
        microphone_muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
    EXPECT_EQ(GetUserPref(), microphone_allowed);
  }
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnAudioNodesChanged) {
  EXPECT_CALL(mock_frontend_, AvailabilityOfMicrophoneChanged(_));
  Shell::Get()
      ->privacy_hub_controller()
      ->microphone_controller()
      .OnAudioNodesChanged();
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnMicrophoneMuteSwitchValueChanged) {
  EXPECT_CALL(mock_frontend_, MicrophoneHardwareToggleChanged(_));
  Shell::Get()
      ->privacy_hub_controller()
      ->microphone_controller()
      .OnInputMutedByMicrophoneMuteSwitchChanged(true);
}

TEST_F(PrivacyHubMicrophoneControllerTest, SimpleMuteUnMute) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Or when we mute.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Or when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, LaunchAppNotUsingMicrophone) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // No notification when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch an app that's not using the mic, should be no notification.
  LaunchApp(absl::nullopt);
  SetNumberOfActiveInputStreams(0);
  EXPECT_FALSE(GetNotification());

  // Mute the mic, still no notification because no app is using the mic.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, LaunchAppUsingMicrophone) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // No notification when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Mute the mic, still no notification.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch an app that's using the mic. The microphone mute notification should
  // show as a popup.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  // Notification should not be pinned.
  EXPECT_FALSE(GetNotification()->rich_notification_data().pinned);

  // Unmute again, notification goes down.
  UnMuteMicrophone();
  WaitUntilNotificationRemoved();
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       SilentNotificationOnMuteWhileMicInUse) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Launch an app that's using the mic, no notification because the microphone
  // is not muted.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  EXPECT_FALSE(GetNotification());

  // Mute the mic, a notification should be shown and also popup.
  MuteMicrophone();
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       ShowPopupNotificationOnStreamAddition) {
  // Launch an app while microphone is muted.
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  ASSERT_TRUE(GetNotification());
  ASSERT_TRUE(GetPopupNotification());

  // Mark the notification as read.
  MarkPopupAsShown();
  ASSERT_FALSE(GetPopupNotification());

  // Add an app, and verify the notification popup gets shown.
  LaunchApp(u"rose");
  SetNumberOfActiveInputStreams(2);

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, RemovingStreamDoesNotShowPopup) {
  // Launch 2 apps while microphone is muted.
  MuteMicrophone();
  LaunchApp(u"junior");
  LaunchApp(u"rose");
  SetNumberOfActiveInputStreams(2);

  ASSERT_TRUE(GetNotification());
  ASSERT_TRUE(GetPopupNotification());

  // Mark the notification as read.
  MarkPopupAsShown();
  ASSERT_FALSE(GetPopupNotification());

  // Remove an active stream, and verify that the notification popup is not
  // reshown.
  SetNumberOfActiveInputStreams(1);

  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  // The notification should be removed if all input streams are removed.
  LaunchApp(absl::nullopt);
  SetNumberOfActiveInputStreams(0);
  WaitUntilNotificationRemoved();

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, SwMuteNotificationActionButton) {
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have an action button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                true),
            0);
  // Clicking the action button should unmute device.
  ClickOnNotificationButton();
  EXPECT_FALSE(CrasAudioHandler::Get()->IsInputMuted());

  EXPECT_FALSE(GetNotification());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                true),
            1);
}

TEST_F(PrivacyHubMicrophoneControllerTest, SwMuteNotificationActionBody) {
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have an action button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            0);

  // Clicking the action button should unmute device.
  ClickOnNotificationBody();
  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());

  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, HwMuteNotificationActionButton) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have a "Learn more" button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Clicking the "Learn more" button should open a new Chrome tab with the
  // support link.
  EXPECT_CALL(new_window_delegate(), OpenUrl).Times(1);
  ClickOnNotificationButton();

  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, HwMuteNotificationActionBody) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  ClickOnNotificationBody();

  // Check that clicking the body has no effect and notification disappears.
  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       TogglingMuteSwitchRemovesNotificationActionButton) {
  // Mute microphone, and activate an audio input stream.
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have an action button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON),
            notification->buttons()[0].title);

  // Toggle microphone mute switch and verify that new notification appears with
  // a "Learn more" button.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE),
            notification->buttons()[0].title);

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  WaitUntilNotificationRemoved();
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       TogglingMuteSwitchDoesNotHideNotificationPopup) {
  // Mute microphone, and activate an audio input stream.
  MuteMicrophone();

  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // Verify the notification popup is shown.
  ASSERT_TRUE(GetNotification());
  ASSERT_TRUE(GetPopupNotification());

  // Toggle microphone mute switch and verify that toggling mute switch alone
  // does not hide the notification popup.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  WaitUntilNotificationRemoved();
  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       RemovingAllInputStreamsWhileHwSwitchToggled) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(2);

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  SetNumberOfActiveInputStreams(0);
  WaitUntilNotificationRemoved();

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest,
       ToggleMicrophoneMuteSwitchWhileInputStreamActive) {
  // Launch an app using microphone, and toggle mute switch.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  // Notification should be shown and also popup.
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  // Add another audio input stream, and verify the notification popup shows.
  LaunchApp(u"junior1");
  SetNumberOfActiveInputStreams(2);

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  // Mark notification as read, and then remove an audio input stream.
  MarkPopupAsShown();
  ASSERT_FALSE(GetPopupNotification());
  SetNumberOfActiveInputStreams(1);

  // Verify that notification popup is not reshown.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  // Adding another stream shows a popup again.
  LaunchApp(u"rose");
  SetNumberOfActiveInputStreams(2);

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
}

TEST_F(PrivacyHubMicrophoneControllerTest, NotificationText) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Mute the mic using sw switch, still no notification.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch an app that's not using the mic, should be no notification.
  LaunchApp(absl::nullopt);
  EXPECT_FALSE(GetNotification());

  // Launch an app that's using the mic, but the name of the app can not be
  // determined.
  LaunchApp(absl::nullopt);
  SetNumberOfActiveInputStreams(1);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
            GetNotification()->title());
  // The notification body should not contain any app name.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE),
      GetNotification()->message());

  // Launch an app that's using the mic, the name of the app can be determined.
  LaunchApp(u"app1");
  SetNumberOfActiveInputStreams(2);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  // The notification body should contain name of the app.
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, u"app1"),
      GetNotification()->message());

  // Launch another app that's using the mic, the name of the app can be
  // determined.
  LaunchApp(u"app2");
  SetNumberOfActiveInputStreams(3);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  // The notification body should contain the two available app names in the
  // order of most recently launched.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                u"app2", u"app1"),
            GetNotification()->message());

  // Launch yet another app that's using the mic, the name of the app can be
  // determined.
  LaunchApp(u"app3");
  SetNumberOfActiveInputStreams(4);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  // As more that two apps are attempting to use the microphone, we fall back to
  // displaying the generic message in the notification.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE),
      GetNotification()->message());

  EXPECT_FALSE(
      ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on());
  // Toggle the hw switch.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());
  // The title of the notification should be different when microphone is muted
  // by the hw switch.
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_HW_SWITCH_NOTIFICATION_TITLE),
            GetNotification()->title());
}

TEST_F(PrivacyHubMicrophoneControllerTest, NotificationUpdatedWhenAppClosed) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Mute the mic using sw switch, still no notification.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch app1 that's accessing the mic, a notification should be displayed
  // with the application name in the notification body.
  const std::u16string app1 = u"app1";
  LaunchApp(app1);
  SetNumberOfActiveInputStreams(1);
  message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app1),
      notification_ptr->message());

  // Launch app2 that's also accessing the mic, the microphone mute notification
  // should be displayed again with both the application names in the
  // notification body.
  const std::u16string app2 = u"app2";
  LaunchApp(app2);
  SetNumberOfActiveInputStreams(2);
  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                app2, app1),
            notification_ptr->message());

  // Close one of the applications. The notification message should be updated
  // to only contain the name of the other application.
  CloseApp(app1);
  SetNumberOfActiveInputStreams(1);
  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app2),
      notification_ptr->message());

  // Test the HW switch notification case.
  // HW switch is turned ON.
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  // Launch the closed app (app1) again.
  LaunchApp(app1);
  SetNumberOfActiveInputStreams(2);
  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                app1, app2),
            notification_ptr->message());

  // Closing one of the applications should remove the name of that application
  // from the hw switch notification message.
  CloseApp(app2);
  SetNumberOfActiveInputStreams(1);
  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app1),
      notification_ptr->message());
}

}  // namespace ash
