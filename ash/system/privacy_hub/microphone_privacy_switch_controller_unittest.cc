// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
      const std::optional<std::u16string> app_name) {
    if (app_name.has_value()) {
      apps_accessing_microphone_.insert(apps_accessing_microphone_.begin(),
                                        app_name.value());
    }
    ++active_input_stream_count_;
    SetActiveInputStreamsCount();
  }

  void CloseAppAccessingMicrophone(const std::u16string& app_name) {
    auto it = base::ranges::find(apps_accessing_microphone_, app_name);
    ASSERT_NE(apps_accessing_microphone_.end(), it);
    apps_accessing_microphone_.erase(it);

    ASSERT_GT(active_input_stream_count_, 0);
    --active_input_stream_count_;
    SetActiveInputStreamsCount();
  }

 private:
  void SetActiveInputStreamsCount() {
    FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
        {{"CRAS_CLIENT_TYPE_CHROME", active_input_stream_count_}});
  }

  int active_input_stream_count_ = 0;
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
  MOCK_METHOD(void, MicrophoneHardwareToggleChanged, (bool), (override));
  MOCK_METHOD(void, SetForceDisableCameraSwitch, (bool), (override));
};

}  // namespace

// Test fixture used to test privacy hub specific behavior.
class PrivacyHubMicrophoneControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyHubMicrophoneControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRef> enabled_features{
        ash::features::kCrosPrivacyHub};
    if (IsVideoConferenceEnabled()) {
      fake_video_conference_tray_controller_ =
          std::make_unique<FakeVideoConferenceTrayController>();
      enabled_features.push_back(features::kFeatureManagementVideoConference);
    }
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(enabled_features, {});
  }
  ~PrivacyHubMicrophoneControllerTest() override {
    fake_video_conference_tray_controller_.reset();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // This makes sure a global instance of `SensorDisabledNotificationDelegate`
    // is created before running tests.
    Shell::Get()->privacy_hub_controller()->SetFrontend(&mock_frontend_);

    // Set up the fake SensorDisabledNotificationDelegate.
    scoped_delegate_ =
        std::make_unique<ScopedSensorDisabledNotificationDelegateForTest>(
            std::make_unique<FakeSensorDisabledNotificationDelegate>());
  }

  void TearDown() override {
    SetMicrophoneMuteSwitchState(/*muted=*/false);

    // We need to destroy the delegate while the Ash still exists.
    scoped_delegate_.reset();
    AshTestBase::TearDown();
  }

  bool IsVideoConferenceEnabled() { return GetParam(); }

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

  bool IsAnyMicNotificationVisible() {
    return GetSWSwitchNotification() != nullptr ||
           GetHWSwitchNotification() != nullptr;
  }

  message_center::Notification* GetSWSwitchNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }

  message_center::Notification* GetHWSwitchNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        PrivacyHubNotificationController::
            kMicrophoneHardwareSwitchNotificationId);
  }

  message_center::Notification* GetSWSwitchPopupNotification() {
    return message_center::MessageCenter::Get()->FindPopupNotificationById(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }

  message_center::Notification* GetHWSwitchPopupNotification() {
    return message_center::MessageCenter::Get()->FindPopupNotificationById(
        PrivacyHubNotificationController::
            kMicrophoneHardwareSwitchNotificationId);
  }

  void MarkPopupAsShown(const std::string& id) {
    message_center::MessageCenter::Get()->MarkSinglePopupAsShown(id, true);
  }

  void ClickOnNotificationButton(const std::string& id) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        id, /*button_index=*/0);
  }

  void ClickOnNotificationBody(const std::string& id) {
    message_center::MessageCenter::Get()->ClickOnNotification(id);
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

  void LaunchApp(std::optional<std::u16string> app_name) {
    sensor_delegate()->LaunchAppAccessingMicrophone(app_name);
  }

  void CloseApp(const std::u16string& app_name) {
    sensor_delegate()->CloseAppAccessingMicrophone(app_name);
  }

  FakeSensorDisabledNotificationDelegate* sensor_delegate() {
    return static_cast<FakeSensorDisabledNotificationDelegate*>(
        PrivacyHubNotificationController::Get()
            ->sensor_disabled_notification_delegate());
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  ::testing::NiceMock<MockFrontendAPI> mock_frontend_;

 private:
  const base::HistogramTester histogram_tester_;
  MockNewWindowDelegate new_window_delegate_;
  std::unique_ptr<FakeVideoConferenceTrayController>
      fake_video_conference_tray_controller_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<ScopedSensorDisabledNotificationDelegateForTest>
      scoped_delegate_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyHubMicrophoneControllerTest,
                         testing::Bool());

TEST_P(PrivacyHubMicrophoneControllerTest, SetSystemMuteOnLogin) {
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

TEST_P(PrivacyHubMicrophoneControllerTest, OnPreferenceChanged) {
  for (bool microphone_allowed : {false, true, false}) {
    SetUserPref(microphone_allowed);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), !microphone_allowed);
  }
}

TEST_P(PrivacyHubMicrophoneControllerTest, OnInputMuteChanged) {
  for (bool microphone_muted : {false, true, false}) {
    const bool microphone_allowed = !microphone_muted;

    CrasAudioHandler::Get()->SetInputMute(
        microphone_muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
    EXPECT_EQ(GetUserPref(), microphone_allowed);
  }
}

TEST_P(PrivacyHubMicrophoneControllerTest, OnMicrophoneMuteSwitchValueChanged) {
  EXPECT_CALL(mock_frontend_, MicrophoneHardwareToggleChanged(_));
  MicrophonePrivacySwitchController::Get()
      ->OnInputMutedByMicrophoneMuteSwitchChanged(true);
}

TEST_P(PrivacyHubMicrophoneControllerTest, SimpleMuteUnMute) {
  // No notification initially.
  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Or when we mute.
  MuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Or when we unmute.
  UnMuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());
}

TEST_P(PrivacyHubMicrophoneControllerTest, LaunchAppUsingMicrophone) {
  // No notification initially.
  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // No notification when we unmute.
  UnMuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Mute the mic, still no notification.
  MuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Launch an app that's using the mic. The microphone mute notification should
  // show as a popup.
  LaunchApp(u"junior");
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
    // Notification should not be pinned.
    EXPECT_FALSE(GetSWSwitchNotification()->rich_notification_data().pinned);
  }
  // Unmute again, notification goes down.
  UnMuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       SilentNotificationOnMuteWhileMicInUse) {
  // No notification initially.
  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Launch an app that's using the mic, no notification because the microphone
  // is not muted.
  LaunchApp(u"junior");

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Mute the mic, no notifications to be shown (as no new apps started, doesn't
  // matter if Video Conference or Privacy Indicators are enabled).
  MuteMicrophone();

  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_FALSE(GetSWSwitchPopupNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       ShowPopupNotificationOnStreamAddition) {
  // Launch an app while microphone is muted.
  MuteMicrophone();
  LaunchApp(u"junior");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
  }
  // Mark the notification as read.
  MarkPopupAsShown(PrivacyHubNotificationController::kCombinedNotificationId);

  EXPECT_FALSE(GetSWSwitchPopupNotification());

  // Add an app, and verify the notification popup gets shown.
  LaunchApp(u"rose");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
  }
}

TEST_P(PrivacyHubMicrophoneControllerTest, RemovingStreamDoesNotShowPopup) {
  // Launch 2 apps while microphone is muted.
  MuteMicrophone();
  LaunchApp(u"junior");
  LaunchApp(u"rose");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
  }

  // Mark the notification as read.
  MarkPopupAsShown(PrivacyHubNotificationController::kCombinedNotificationId);

  ASSERT_FALSE(GetSWSwitchPopupNotification());

  // Close an active app, and verify that the notification popup is not
  // reshown.
  CloseApp(u"rose");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
  }
  EXPECT_FALSE(GetSWSwitchPopupNotification());

  // The notification should be removed if all apps are closed.
  CloseApp(u"junior");

  EXPECT_FALSE(GetSWSwitchNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest, SwMuteNotificationActionButton) {
  MuteMicrophone();
  LaunchApp(u"junior");

  // The mute notification should have an action button.
  message_center::Notification* notification = GetSWSwitchNotification();

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
    return;
  }

  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Clicking the action button should unmute device.
  ClickOnNotificationButton(
      PrivacyHubNotificationController::kCombinedNotificationId);

  EXPECT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                true),
            1);
}

TEST_P(PrivacyHubMicrophoneControllerTest, SwMuteNotificationActionBody) {
  MuteMicrophone();
  LaunchApp(u"junior");

  // The mute notification should have an action button.
  message_center::Notification* notification = GetSWSwitchNotification();

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
    return;
  }

  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Clicking the action button should unmute device.
  ClickOnNotificationBody(
      PrivacyHubNotificationController::kCombinedNotificationId);

  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetSWSwitchNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest, HwMuteNotificationActionButton) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  LaunchApp(u"junior");

  // The hardware switch notification should be displayed. The notification
  // should have a "Learn more" button.
  EXPECT_FALSE(GetSWSwitchNotification());
  message_center::Notification* notification = GetHWSwitchNotification();

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
    return;
  }

  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Clicking the "Learn more" button should open a new Chrome tab with the
  // support link.
  EXPECT_CALL(new_window_delegate(), OpenUrl).Times(1);
  ClickOnNotificationButton(PrivacyHubNotificationController::
                                kMicrophoneHardwareSwitchNotificationId);

  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());

  SetMicrophoneMuteSwitchState(/*muted=*/false);

  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(IsAnyMicNotificationVisible());
}

TEST_P(PrivacyHubMicrophoneControllerTest, HwMuteNotificationActionBody) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  LaunchApp(u"junior");

  message_center::Notification* notification = GetHWSwitchNotification();

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
    return;
  }

  ASSERT_TRUE(notification);

  // Check that clicking the body has no effect but notification disappears.
  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());
  ClickOnNotificationBody(PrivacyHubNotificationController::
                              kMicrophoneHardwareSwitchNotificationId);

  EXPECT_TRUE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetHWSwitchNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       TogglingMuteSwitchRemovesNotificationActionButton) {
  // Mute microphone, and activate an audio input stream.
  MuteMicrophone();
  LaunchApp(u"junior");

  // The mute notification should have an action button.
  message_center::Notification* notification = GetSWSwitchNotification();

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);
    EXPECT_EQ(1u, notification->buttons().size());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON),
              notification->buttons()[0].title);
  }
  // Toggle microphone mute switch and verify that new notification appears
  // with a "Learn more" button.
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  EXPECT_FALSE(GetSWSwitchNotification());

  notification = GetHWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);
    EXPECT_EQ(1u, notification->buttons().size());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE),
              notification->buttons()[0].title);
  }

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());

  EXPECT_FALSE(IsAnyMicNotificationVisible());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       TogglingMuteSwitchDoesNotHideNotificationPopup) {
  // Mute microphone, and activate an audio input stream.
  MuteMicrophone();
  LaunchApp(u"junior");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    // Verify the notification popup is shown.
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
  }

  // Toggle microphone mute switch and verify that toggling mute switch creates
  // new hardware switch pop up notification and the software switch
  // notification is removed.
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
  } else {
    // Verify the notification popup is shown.
    EXPECT_TRUE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
  }

  // The software switch notification is instantly hidden.
  EXPECT_FALSE(GetSWSwitchNotification());

  // Toggling the mute switch again should remove all microphone mute
  // notifications.
  SetMicrophoneMuteSwitchState(/*muted=*/false);

  ASSERT_FALSE(CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(IsAnyMicNotificationVisible());
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_FALSE(GetHWSwitchNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       RemovingAllInputStreamsWhileHwSwitchToggled) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  LaunchApp(u"junior");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchNotification());
  } else {
    EXPECT_TRUE(GetHWSwitchNotification());
    EXPECT_TRUE(GetHWSwitchPopupNotification());
  }

  CloseApp(u"junior");

  EXPECT_FALSE(GetHWSwitchNotification());
}

TEST_P(PrivacyHubMicrophoneControllerTest,
       ToggleMicrophoneMuteSwitchWhileInputStreamActive) {
  // Launch an app using microphone, and toggle mute switch.
  LaunchApp(u"junior");
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  // No notifications to be shown if microphone was turned off.
  EXPECT_FALSE(GetHWSwitchNotification());
  EXPECT_FALSE(GetHWSwitchPopupNotification());

  // Add another audio input stream, and verify the notification popup shows.
  LaunchApp(u"junior1");

  if (IsVideoConferenceEnabled()) {
    // If an app started while video conference is on - no notifications.
    EXPECT_FALSE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
  } else {
    // Otherwise - it shall be a notification.
    EXPECT_TRUE(GetHWSwitchNotification());
    EXPECT_TRUE(GetHWSwitchPopupNotification());
  }

  // Mark notification as read, and then remove an audio input stream.
  MarkPopupAsShown(PrivacyHubNotificationController::
                       kMicrophoneHardwareSwitchNotificationId);

  ASSERT_FALSE(GetHWSwitchPopupNotification());

  CloseApp(u"junior1");

  // Verify that notification popup is not reshown.
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetHWSwitchNotification());
  } else {
    EXPECT_TRUE(GetHWSwitchNotification());
  }
  EXPECT_FALSE(GetHWSwitchPopupNotification());

  // Adding another stream shows a popup again.
  LaunchApp(u"rose");
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetHWSwitchNotification());
    EXPECT_TRUE(GetHWSwitchPopupNotification());
  }
}

TEST_P(PrivacyHubMicrophoneControllerTest, NotificationText) {
  // No notification initially.
  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Mute the mic using sw switch, still no notification.
  MuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Launch an app that's using the mic, but the name of the app can not be
  // determined.
  LaunchApp(std::nullopt);

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
              GetSWSwitchNotification()->title());
    // The notification body should not contain any app name.
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE),
        GetSWSwitchNotification()->message());
  }

  // Launch an app that's using the mic, the name of the app can be determined.
  LaunchApp(u"app1");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
    // The notification body should contain name of the app.
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
                  u"app1"),
              GetSWSwitchNotification()->message());
  }

  // Launch another app that's using the mic, the name of the app can be
  // determined.
  LaunchApp(u"app2");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
    // The notification body should contain the two available app names in the
    // order of most recently launched.
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                  u"app1", u"app2"),
              GetSWSwitchNotification()->message());
  }

  // Launch yet another app that's using the mic, the name of the app can be
  // determined.
  LaunchApp(u"app3");

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    EXPECT_FALSE(GetSWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
    EXPECT_TRUE(GetSWSwitchPopupNotification());
    // As more that two apps are attempting to use the microphone, we fall back
    // to displaying the generic message in the notification.
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE),
        GetSWSwitchNotification()->message());
  }

  EXPECT_FALSE(
      ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on());
  EXPECT_FALSE(GetHWSwitchNotification());

  // Toggle the hw switch.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
  } else {
    EXPECT_TRUE(GetHWSwitchNotification());
    EXPECT_FALSE(GetHWSwitchPopupNotification());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_MICROPHONE_MUTED_BY_HW_SWITCH_NOTIFICATION_TITLE),
              GetHWSwitchNotification()->title());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE),
        GetHWSwitchNotification()->message());
  }
}

TEST_P(PrivacyHubMicrophoneControllerTest, NotificationUpdatedWhenAppClosed) {
  // No notification initially.
  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Mute the mic using sw switch, still no notification.
  MuteMicrophone();

  EXPECT_FALSE(IsAnyMicNotificationVisible());

  // Launch app1 that's accessing the mic, a notification should be displayed
  // with the application name in the notification body.
  const std::u16string app1 = u"app1";
  LaunchApp(app1);

  message_center::Notification* notification_ptr = GetSWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification_ptr);
  } else {
    ASSERT_TRUE(notification_ptr);
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app1),
        notification_ptr->message());
  }

  // Launch app2 that's also accessing the mic, the microphone mute notification
  // should be displayed again with both the application names in the
  // notification body.
  const std::u16string app2 = u"app2";
  LaunchApp(app2);

  notification_ptr = GetSWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification_ptr);
  } else {
    ASSERT_TRUE(notification_ptr);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                  app1, app2),
              notification_ptr->message());
  }

  // Close one of the applications. The notification message should be updated
  // to only contain the name of the other application.
  CloseApp(app1);

  notification_ptr = GetSWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification_ptr);
  } else {
    ASSERT_TRUE(notification_ptr);
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app2),
        notification_ptr->message());
  }

  // Test the HW switch notification case.
  // HW switch is turned ON.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  // Launch the closed app (app1) again.
  LaunchApp(app1);

  notification_ptr = GetHWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification_ptr);
  } else {
    ASSERT_TRUE(notification_ptr);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
                  app1, app2),
              notification_ptr->message());
  }

  // Closing one of the applications should remove the name of that application
  // from the hw switch notification message.
  CloseApp(app2);

  notification_ptr = GetHWSwitchNotification();
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(notification_ptr);
  } else {
    ASSERT_TRUE(notification_ptr);
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME, app1),
        notification_ptr->message());
  }
}

}  // namespace ash
