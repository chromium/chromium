// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;

namespace ash {

namespace {

using Sensor = SensorDisabledNotificationDelegate::Sensor;

class MockSwitchAPI : public CameraPrivacySwitchAPI {
 public:
  MOCK_METHOD(void,
              SetCameraSWPrivacySwitch,
              (CameraSWPrivacySwitchSetting),
              (override));
};

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    if (sensor == Sensor::kCamera) {
      return apps_accessing_camera_;
    }
    return {};
  }

  void LaunchAppAccessingCamera(const std::u16string& app_name) {
    apps_accessing_camera_.insert(apps_accessing_camera_.begin(), app_name);
  }

  void CloseAppAccessingCamera(const std::u16string& app_name) {
    auto it = base::ranges::find(apps_accessing_camera_, app_name);
    if (it != apps_accessing_camera_.end()) {
      apps_accessing_camera_.erase(it);
    }
  }

 private:
  std::vector<std::u16string> apps_accessing_camera_;
};

message_center::Notification* FindNotificationById(const std::string& id) {
  return message_center::MessageCenter::Get()->FindNotificationById(id);
}

// Mutes the camera on construction, and unmutes the camera when the object goes
// out of scope.
class ScopedCameraMuteToggler {
 public:
  explicit ScopedCameraMuteToggler(bool software_switch)
      : camera_privacy_switch_controller_(
            *CameraPrivacySwitchController::Get()),
        software_switch_(software_switch) {
    if (software_switch_) {
      Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
          prefs::kUserCameraAllowed, /*value=*/false);
    } else {
      camera_privacy_switch_controller_->OnCameraHWPrivacySwitchStateChanged(
          std::string(), cros::mojom::CameraPrivacySwitchState::ON);
    }
  }

  ~ScopedCameraMuteToggler() {
    if (software_switch_) {
      Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
          prefs::kUserCameraAllowed, /*value=*/true);
    } else {
      camera_privacy_switch_controller_->OnCameraHWPrivacySwitchStateChanged(
          std::string(), cros::mojom::CameraPrivacySwitchState::OFF);
    }
  }

 private:
  const raw_ref<CameraPrivacySwitchController>
      camera_privacy_switch_controller_;
  const bool software_switch_;
};

class MockFrontendAPI : public PrivacyHubDelegate {
 public:
  MOCK_METHOD(void, MicrophoneHardwareToggleChanged, (bool), (override));
  MOCK_METHOD(void, SetForceDisableCameraSwitch, (bool), (override));
};

}  // namespace

class PrivacyHubCameraTestBase : public AshTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  PrivacyHubCameraTestBase()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsVideoConferenceEnabled()) {
      fake_video_conference_tray_controller_ =
          std::make_unique<FakeVideoConferenceTrayController>();
      enabled_features.push_back(features::kFeatureManagementVideoConference);
    } else {
      disabled_features.push_back(features::kFeatureManagementVideoConference);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~PrivacyHubCameraTestBase() override {
    fake_video_conference_tray_controller_.reset();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto mock_switch = std::make_unique<::testing::NiceMock<MockSwitchAPI>>();
    mock_switch_ = mock_switch.get();

    PrivacyHubController::Get()
        ->camera_controller()
        ->SetCameraPrivacySwitchAPIForTest(std::move(mock_switch));

    // Set up the fake `SensorDisabledNotificationDelegate`.
    // In production it is set only if Privacy Hub is enabled.
    scoped_delegate_ =
        std::make_unique<ScopedSensorDisabledNotificationDelegateForTest>(
            std::make_unique<FakeSensorDisabledNotificationDelegate>());
  }

  void TearDown() override {
    // We need to destroy the delegate while the Ash still exists.
    scoped_delegate_.reset();
    AshTestBase::TearDown();
  }

  bool IsVideoConferenceEnabled() { return GetParam(); }

  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserCameraAllowed, allowed);
  }

  bool GetUserPref() const {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserCameraAllowed);
  }

 protected:
  raw_ptr<::testing::NiceMock<MockSwitchAPI>, DanglingUntriaged> mock_switch_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScopedSensorDisabledNotificationDelegateForTest>
      scoped_delegate_;

  std::unique_ptr<FakeVideoConferenceTrayController>
      fake_video_conference_tray_controller_;
};

using PrivacyHubCameraSynchronizerTest = PrivacyHubCameraTestBase;

// Test reaction on user pref change (e.g. in UI).
TEST_P(PrivacyHubCameraSynchronizerTest, UserPrefChange) {
  const std::vector<bool> user_pref_sequence{false, true, true, false, true};
  const int number_of_changes = [&]() {
    int cnt = 0;
    bool current_val = true;  // default value for camera-enabled is true
    for (const bool pref_val : user_pref_sequence) {
      if (pref_val != current_val) {
        ++cnt;
        current_val = pref_val;
      }
    }
    return cnt;
  }();

  CameraSWPrivacySwitchSetting captured_val;
  EXPECT_CALL(*mock_switch_, SetCameraSWPrivacySwitch(_))
      .Times(number_of_changes)
      .WillRepeatedly(testing::SaveArg<0>(&captured_val));

  for (const bool pref_val : user_pref_sequence) {
    SetUserPref(pref_val);
    // User toggle ON means the camera is DISABLED.
    const CameraSWPrivacySwitchSetting expected_val =
        pref_val ? CameraSWPrivacySwitchSetting::kEnabled
                 : CameraSWPrivacySwitchSetting::kDisabled;
    EXPECT_EQ(captured_val, expected_val);
  }
}

TEST_P(PrivacyHubCameraSynchronizerTest, OnCameraSoftwarePrivacySwitchChanged) {
  // When `prefs::kUserCameraAllowed` is true and CrOS Camera Service
  // communicates the SW privacy switch state as UNKNOWN or ON, the states
  // mismatch and `SetCameraSWPrivacySwitch(kEnabled)` should be called to
  // correct the mismatch.
  EXPECT_CALL(*mock_switch_,
              SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting::kEnabled))
      .Times(::testing::Exactly(3));
  SetUserPref(true);
  PrivacyHubController::Get()
      ->camera_controller()
      ->OnCameraSWPrivacySwitchStateChanged(
          cros::mojom::CameraPrivacySwitchState::UNKNOWN);
  PrivacyHubController::Get()
      ->camera_controller()
      ->OnCameraSWPrivacySwitchStateChanged(
          cros::mojom::CameraPrivacySwitchState::ON);

  if (IsVideoConferenceEnabled()) {
    // When `prefs::kUserCameraAllowed` is false and CrOS Camera Service
    // communicates the SW privacy switch state as UNKNOWN or OFF, the states
    // mismatch and `SetCameraSWPrivacySwitch(kDisabled)` should be called to
    // correct the mismatch.
    EXPECT_CALL(*mock_switch_, SetCameraSWPrivacySwitch(
                                   CameraSWPrivacySwitchSetting::kDisabled))
        .Times(::testing::Exactly(3));
    SetUserPref(false);
    PrivacyHubController::Get()
        ->camera_controller()
        ->OnCameraSWPrivacySwitchStateChanged(
            cros::mojom::CameraPrivacySwitchState::UNKNOWN);
    PrivacyHubController::Get()
        ->camera_controller()
        ->OnCameraSWPrivacySwitchStateChanged(
            cros::mojom::CameraPrivacySwitchState::OFF);

    // When the SW privacy switch states match in Privacy Hub and CrOS Camera
    // Service, `SetCameraSWPrivacySwitch()` should not be called.
    EXPECT_CALL(*mock_switch_, SetCameraSWPrivacySwitch(_))
        .Times(::testing::Exactly(2));

    // When `prefs::kUserCameraAllowed` is true and CrOS Camera Service
    // communicates the SW privacy switch state as OFF, the states match and
    // `SetCameraSWPrivacySwitch()` should not be called.
    SetUserPref(true);
    PrivacyHubController::Get()
        ->camera_controller()
        ->OnCameraSWPrivacySwitchStateChanged(
            cros::mojom::CameraPrivacySwitchState::OFF);

    // When `prefs::kUserCameraAllowed` is false and CrOS Camera Service
    // communicates the SW privacy switch state as ON, the states match and
    // `SetCameraSWPrivacySwitch()` should not be called.
    SetUserPref(false);
    PrivacyHubController::Get()
        ->camera_controller()
        ->OnCameraSWPrivacySwitchStateChanged(
            cros::mojom::CameraPrivacySwitchState::ON);
  }
}

class NotificationTestBase : public PrivacyHubCameraTestBase {
 public:
  void SetUp() override {
    PrivacyHubCameraTestBase::SetUp();
    controller_ = CameraPrivacySwitchController::Get();
  }

  void LaunchAppAccessingCamera(const std::u16string& app_name) {
    delegate()->LaunchAppAccessingCamera(app_name);
    controller_->ActiveApplicationsChanged(/*application_added=*/true);
  }

  void CloseAppAccessingCamera(const std::u16string& app_name) {
    delegate()->CloseAppAccessingCamera(app_name);
    controller_->ActiveApplicationsChanged(/*application_added=*/false);
  }

  message_center::Notification* GetSWSwitchNotification() {
    return FindNotificationById(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }

  FakeSensorDisabledNotificationDelegate* delegate() {
    return static_cast<FakeSensorDisabledNotificationDelegate*>(
        PrivacyHubNotificationController::Get()
            ->sensor_disabled_notification_delegate());
  }

  raw_ptr<CameraPrivacySwitchController, DanglingUntriaged> controller_;
  const base::HistogramTester histogram_tester_;
};

class PrivacyHubCameraControllerTest : public NotificationTestBase {};

TEST_P(PrivacyHubCameraControllerTest,
       OnCameraHardwarePrivacySwitchChangedMultipleCameras) {
  CameraPrivacySwitchController& controller =
      *CameraPrivacySwitchController::Get();

  // We have 2 cameras in the system.
  controller.OnCameraCountChanged(2);
  // Camera is enabled in Privacy Hub.
  SetUserPref(true);

  // Somebody switched the camera off by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::OFF);
  // Is shall be no SW notification.
  EXPECT_FALSE(GetSWSwitchNotification());

  // Somebody switched the camera on by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::ON);
  // Is shall be no SW notification.
  EXPECT_FALSE(GetSWSwitchNotification());

  // User pref didn't change.
  EXPECT_TRUE(GetUserPref());
  // We didn't log any notification clicks so far.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                false),
            0);
}

TEST_P(PrivacyHubCameraControllerTest,
       OnCameraHardwarePrivacySwitchChangedOneCamera) {
  CameraPrivacySwitchController& controller =
      *CameraPrivacySwitchController::Get();
  // We have 1 camera in the system.
  controller.OnCameraCountChanged(1);
  // Camera is enabled in Privacy Hub.
  SetUserPref(true);

  // Somebody switched the camera off by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::OFF);
  // Is shall be no SW notification.
  EXPECT_FALSE(GetSWSwitchNotification());

  // Switching the hardware switch back again.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::ON);
  // Is shall be no SW notification.
  EXPECT_FALSE(GetSWSwitchNotification());

  // This didn't cause any change in the setting toggle.
  EXPECT_TRUE(GetUserPref());
  // There were no changes to the histograms.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                false),
            0);
}

// This test is a regression test for b/253407315
TEST_P(PrivacyHubCameraControllerTest,
       OnCameraHardwarePrivacySwitchChangedNotificationClearing) {
  CameraPrivacySwitchController& controller =
      *CameraPrivacySwitchController::Get();
  SetUserPref(true);
  controller.OnCameraCountChanged(2);

  controller.OnCameraHWPrivacySwitchStateChanged(
      "0", cros::mojom::CameraPrivacySwitchState::ON);

  LaunchAppAccessingCamera(u"app_name");

  // Notification should be cleared when hardware mute is disabled
  controller.OnCameraHWPrivacySwitchStateChanged(
      "0", cros::mojom::CameraPrivacySwitchState::OFF);

  // Parameters shall not be changed
  EXPECT_TRUE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest,
       CameraOffNotificationRemoveViaClickOnButton) {
  SetUserPref(false);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(GetSWSwitchNotification());

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  if (IsVideoConferenceEnabled()) {
    // No Notification should be sent. Return early because the rest of the test
    // tests notification behavior.
    EXPECT_FALSE(GetSWSwitchNotification());
    return;
  }
  // A notification should be fired.
  EXPECT_TRUE(GetSWSwitchNotification());
  EXPECT_FALSE(GetUserPref());

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  // Enabling camera via clicking on the button should clear the notification
  message_center->ClickOnNotificationButton(
      PrivacyHubNotificationController::kCombinedNotificationId, 0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
}

TEST_P(PrivacyHubCameraControllerTest,
       CameraOffNotificationRemoveViaClickOnBody) {
  SetUserPref(false);
  controller_->OnCameraCountChanged(2);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(GetSWSwitchNotification());

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
  } else {
    // A notification should be fired.
    EXPECT_TRUE(GetSWSwitchNotification());
  }
  EXPECT_FALSE(GetUserPref());

  if (!IsVideoConferenceEnabled()) {
    // Enabling camera via clicking on the body should open the privacy hub
    // settings page.
    message_center->ClickOnNotification(
        PrivacyHubNotificationController::kCombinedNotificationId);

    // The user pref should not be changed.
    EXPECT_FALSE(GetUserPref());
    EXPECT_FALSE(GetSWSwitchNotification());
  }

  SetUserPref(true);

  // Flip the hardware switch.
  controller_->OnCameraHWPrivacySwitchStateChanged(
      "0", cros::mojom::CameraPrivacySwitchState::ON);

  // No notification is fired for switch changes during the capture session.
  // But one will be fired if a new session starts.
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_TRUE(GetUserPref());

  // Adds a second application.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  // Is shall be no SW notification.
  EXPECT_FALSE(GetSWSwitchNotification());

  EXPECT_TRUE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest, CameraOffNotificationRemoveViaUserPref) {
  SetUserPref(false);
  ASSERT_FALSE(GetSWSwitchNotification());

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
  } else {
    // A notification should be fired.
    EXPECT_TRUE(GetSWSwitchNotification());
  }
  EXPECT_FALSE(GetUserPref());

  // Enabling camera via the user pref should clear the notification.
  SetUserPref(true);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(GetSWSwitchNotification());
}

TEST_P(PrivacyHubCameraControllerTest, InSessionSwitchNotification) {
  SetUserPref(true);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  message_center->RemoveNotification(
      PrivacyHubNotificationController::kCombinedNotificationId, false);

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  // Disable the camera after the application count has changed.
  SetUserPref(false);
  // A notification should not be fired (when pref set to false while an app was
  // running).
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_FALSE(GetUserPref());

  // Trigger the notification by simulating a second application accessing the
  // camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  // If VC enabled - no notification to be shown.
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
    // No notifications shown - nothing left to test.
    return;
  }

  // Otherwise - a notification to be shown if a new app started.
  EXPECT_TRUE(GetSWSwitchNotification());
  EXPECT_FALSE(GetUserPref());

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  // Enabling camera via clicking on the button should clear the notification.
  message_center->ClickOnNotificationButton(
      PrivacyHubNotificationController::kCombinedNotificationId, 0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(GetSWSwitchNotification());
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
}

// Tests if camera software switch notification is removed when the number of
// apps accessing the camera becomes 0.
TEST_P(PrivacyHubCameraControllerTest,
       NotificationRemovedWhenNoActiveApplication) {
  SetUserPref(true);

  // The notification should not be in the message center initially.
  EXPECT_FALSE(GetSWSwitchNotification());

  // This is the effect of an application starting to access the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  // Disabling camera using the software switch.
  SetUserPref(false);

  // No notification pops up if just the switch is modified.
  EXPECT_FALSE(GetSWSwitchNotification());

  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
  }

  // The only active application stops accessing the camera the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/false);

  // The notification should still be shown, because two apps were accessing
  // the camera.
  if (IsVideoConferenceEnabled()) {
    EXPECT_FALSE(GetSWSwitchNotification());
  } else {
    EXPECT_TRUE(GetSWSwitchNotification());
  }

  controller_->ActiveApplicationsChanged(/*application_added=*/false);

  // Existing notification should be removed as the number of active
  // applications is 0 now.
  EXPECT_FALSE(GetSWSwitchNotification());
}

// Tests if the camera software switch notification contains proper text.
TEST_P(PrivacyHubCameraControllerTest, NotificationText) {
  if (IsVideoConferenceEnabled()) {
    return;
  }
  // Disabling camera using the software switch.
  SetUserPref(false);
  EXPECT_FALSE(GetSWSwitchNotification());

  // Launch app1 that's accessing camera, a notification should be displayed
  // with the application name in the notification body.
  const std::u16string app1 = u"app1";
  LaunchAppAccessingCamera(app1);

  message_center::Notification* notification = GetSWSwitchNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE),
      notification->title());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          app1),
      notification->message());

  // Launch app2 that's also accessing camera, a notification should be
  // displayed again with both of the application names in the notification
  // body.
  const std::u16string app2 = u"app2";
  LaunchAppAccessingCamera(app2);

  notification = GetSWSwitchNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app1, app2),
      notification->message());

  // Launch app3 that's also accessing camera, a notification should be
  // displayed again with generic text.
  const std::u16string app3 = u"app3";
  LaunchAppAccessingCamera(app3);

  notification = GetSWSwitchNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE),
            notification->message());

  // Close one of the applications. The notification should be updated to
  // contain the name of the two remaining applications.
  CloseAppAccessingCamera(app2);

  notification = GetSWSwitchNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app1, app3),
      notification->message());
}

TEST_P(PrivacyHubCameraControllerTest, MetricCollection) {
  if (IsVideoConferenceEnabled()) {
    return;
  }
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                false),
            0);

  PrivacyHubNotificationController::SetAndLogSensorPreferenceFromNotification(
      Sensor::kCamera, false);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                false),
            1);

  PrivacyHubNotificationController::SetAndLogSensorPreferenceFromNotification(
      Sensor::kCamera, true);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                false),
            1);
}

TEST_P(PrivacyHubCameraControllerTest,
       ForceDisableAccessShouldDisableUserPref) {
  SetUserPref(true);

  auto& controller = *CameraPrivacySwitchController::Get();

  controller.SetForceDisableCameraAccess(true);

  EXPECT_FALSE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest,
       ForceDisableAccessShouldAlsoWorkBeforeUserPrefIsRegistered) {
  SetUserPref(true);

  // Create a local version of the controller, so we can delay registration of
  // the user pref service.
  CameraPrivacySwitchController controller;

  controller.SetForceDisableCameraAccess(true);

  controller.OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  EXPECT_FALSE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest,
       ForceDisableAccessShouldPreventPrefChanges) {
  auto& controller = *CameraPrivacySwitchController::Get();
  controller.SetForceDisableCameraAccess(true);

  SetUserPref(true);
  EXPECT_FALSE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest, ShouldBeAbleToStopForceDisableAccess) {
  auto& controller = *CameraPrivacySwitchController::Get();

  controller.SetForceDisableCameraAccess(true);
  ASSERT_TRUE(controller.IsCameraAccessForceDisabled());

  controller.SetForceDisableCameraAccess(false);
  ASSERT_FALSE(controller.IsCameraAccessForceDisabled());
}

TEST_P(PrivacyHubCameraControllerTest,
       ShouldBeAbleToChangePrefAfterStoppingForceDisableAccess) {
  auto& controller = *CameraPrivacySwitchController::Get();
  controller.SetForceDisableCameraAccess(true);
  controller.SetForceDisableCameraAccess(false);

  SetUserPref(true);
  EXPECT_TRUE(GetUserPref());
}

TEST_P(PrivacyHubCameraControllerTest,
       StoppingForceDisableAccessShouldNotCrashEvenBeforeUserPrefIsRegistered) {
  // Create a local version of the controller, so we can delay registration of
  // the user pref service.
  CameraPrivacySwitchController controller;

  // Neither of these should crash (by trying to access the user pref).
  controller.SetForceDisableCameraAccess(true);
  controller.SetForceDisableCameraAccess(false);
}

TEST_P(PrivacyHubCameraControllerTest,
       ShouldRestorePreviousPrefAfterForceDisableAccess) {
  auto& controller = *CameraPrivacySwitchController::Get();

  for (const bool previous_value : {true, false}) {
    SetUserPref(previous_value);
    controller.SetForceDisableCameraAccess(true);
    controller.SetForceDisableCameraAccess(false);
    EXPECT_EQ(GetUserPref(), previous_value);
  }
}

TEST_P(PrivacyHubCameraControllerTest,
       ShouldRestorePreviousPrefEvenAfterRecreatingTheController) {
  // This test ensures that we restore the previous pref value, even if
  // it was force disabled and then Chrome crashed.

  for (const bool previous_value : {true, false}) {
    SetUserPref(previous_value);

    std::optional<CameraPrivacySwitchController> controller;

    // Create the controller.
    controller.emplace();
    controller->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    // Force disable camera access.
    controller->SetForceDisableCameraAccess(true);

    // Simulate crash and restart by destroying and recreating the controller.
    controller.emplace();
    controller->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    // The previous pref value should be restored.
    EXPECT_EQ(GetUserPref(), previous_value);
  }
}

TEST_P(PrivacyHubCameraControllerTest,
       ForceDisableAccessShouldDisableUiSwitch) {
  ::testing::StrictMock<MockFrontendAPI> mock_frontend;
  Shell::Get()->privacy_hub_controller()->SetFrontend(&mock_frontend);
  auto& controller = *CameraPrivacySwitchController::Get();

  EXPECT_CALL(mock_frontend, SetForceDisableCameraSwitch(true));

  controller.SetForceDisableCameraAccess(true);

  Shell::Get()->privacy_hub_controller()->SetFrontend(nullptr);
}

TEST_P(PrivacyHubCameraControllerTest,
       StoppingForceDisableAccessShouldReenableUiSwitch) {
  ::testing::StrictMock<MockFrontendAPI> mock_frontend;
  Shell::Get()->privacy_hub_controller()->SetFrontend(&mock_frontend);
  auto& controller = *CameraPrivacySwitchController::Get();

  EXPECT_CALL(mock_frontend, SetForceDisableCameraSwitch(false));

  controller.SetForceDisableCameraAccess(false);

  Shell::Get()->privacy_hub_controller()->SetFrontend(nullptr);
}

TEST_P(PrivacyHubCameraControllerTest,
       ShouldRestorePreviousPrefEvenAfterForceDisableAccessTwice) {
  // This test ensures that force disabling camera access a second time will
  // not overwrite the stored previous value (which should keep reflecting the
  // state from before we force disabled camera access the first time).

  for (const bool previous_value : {true, false}) {
    SetUserPref(previous_value);

    std::optional<CameraPrivacySwitchController> controller;

    // Create the controller.
    controller.emplace();
    controller->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    // Force disable camera access.
    controller->SetForceDisableCameraAccess(true);
    // Twice.
    controller->SetForceDisableCameraAccess(true);

    // Simulate crash and restart by destroying and recreating the controller.
    controller.emplace();
    controller->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    // The previous pref value should be restored.
    EXPECT_EQ(GetUserPref(), previous_value);
  }
}

class VideoConferenceCameraControllerTest : public NotificationTestBase {};

// With VcControls enabled, tests that no notification shows up if the switches
// are toggled when the number of capturing apps does not change.
TEST_P(VideoConferenceCameraControllerTest,
       NoNotificationDuringCaptureSession) {
  if (!IsVideoConferenceEnabled()) {
    return;
  }

  const std::u16string app_name = u"app_name";
  for (bool app_running : {false, true}) {
    if (app_running) {
      LaunchAppAccessingCamera(app_name);
      EXPECT_FALSE(GetSWSwitchNotification());
    }
    // Disable camera using the software switch. No notification should show.
    SetUserPref(/*allowed=*/false);

    EXPECT_FALSE(GetSWSwitchNotification());

    // Re-enable the camera, there still should be no notification.
    SetUserPref(/*allowed=*/true);

    EXPECT_FALSE(GetSWSwitchNotification());

    // Repeat the test with the hardware switch.
    auto* controller = CameraPrivacySwitchController::Get();
    controller->OnCameraHWPrivacySwitchStateChanged(
        std::string(), cros::mojom::CameraPrivacySwitchState::ON);

    // It shall not cause SW notification
    EXPECT_FALSE(GetSWSwitchNotification());

    controller->OnCameraHWPrivacySwitchStateChanged(
        std::string(), cros::mojom::CameraPrivacySwitchState::OFF);

    // It shall not cause SW notification
    EXPECT_FALSE(GetSWSwitchNotification());

    if (app_running) {
      CloseAppAccessingCamera(app_name);
    }
  }
}

// With VcControls or Privacy Indicators enabled, tests that a notification
// shows up when the number of apps capturing the camera increases if the switch
// is muted.
TEST_P(VideoConferenceCameraControllerTest,
       NotificationShowsIfNewAppStartsCapturing) {
  for (bool software_switch : {true, false}) {
    SCOPED_TRACE(::testing::Message()
                 << "software_switch: " << software_switch);
    auto scoped_software_switch_toggler =
        ScopedCameraMuteToggler(/*software_switch=*/software_switch);

    // independently from the software_switch - shall be no notification.
    EXPECT_FALSE(GetSWSwitchNotification());

    // Simulate an app accessing the camera, a notification should show.
    LaunchAppAccessingCamera(u"app_name");

    if (IsVideoConferenceEnabled()) {
      // No notification when VC is enabled.
      EXPECT_FALSE(GetSWSwitchNotification());
    } else {
      // No VC - notification to be shown only for SW switch.
      if (software_switch) {
        EXPECT_TRUE(GetSWSwitchNotification());
      } else {
        EXPECT_FALSE(GetSWSwitchNotification());
      }
    }
  }
}

// With VcControls or Privacy Indicators enabled, tests that turning camera
// access back on hides the notification.
TEST_P(VideoConferenceCameraControllerTest,
       EnablingCameraAccessHidesNotification) {
  for (bool software_switch : {true, false}) {
    auto scoped_software_switch_toggler =
        std::make_unique<ScopedCameraMuteToggler>(
            /*software_switch=*/software_switch);

    // Simulate an app accessing the camera.
    LaunchAppAccessingCamera(u"app_name");

    if (IsVideoConferenceEnabled()) {
      // No notification when VC is enabled.
      EXPECT_FALSE(GetSWSwitchNotification());
    } else {
      // No VC - notification to be shown only for SW switch.
      if (software_switch) {
        EXPECT_TRUE(GetSWSwitchNotification());
      } else {
        EXPECT_FALSE(GetSWSwitchNotification());
      }
    }
    // Reverses the switch, the notification should go away.
    scoped_software_switch_toggler.reset();

    EXPECT_FALSE(GetSWSwitchNotification());
  }
}

// With VcControls or Privacy Indicators enabled, tests that a notification
// shows up if muted and then a capture starts.
TEST_P(VideoConferenceCameraControllerTest,
       NotificationWhenMutedOnCaptureStart) {
  for (bool software_switch : {true, false}) {
    auto scoped_software_switch_toggler =
        std::make_unique<ScopedCameraMuteToggler>(
            /*software_switch=*/software_switch);
    const std::u16string app_name = u"app_name";
    LaunchAppAccessingCamera(app_name);

    if (IsVideoConferenceEnabled()) {
      // No notification when VC is enabled.
      EXPECT_FALSE(GetSWSwitchNotification());
    } else {
      // No VC - notification to be shown only for SW switch.
      if (software_switch) {
        EXPECT_TRUE(GetSWSwitchNotification());
      } else {
        EXPECT_FALSE(GetSWSwitchNotification());
      }
    }

    CloseAppAccessingCamera(app_name);

    EXPECT_FALSE(GetSWSwitchNotification());
  }
}

// With VcControls or Privacy Indicators enabled, tests that a notification goes
// away when capturing apps drop to 0.
TEST_P(VideoConferenceCameraControllerTest,
       NotificationGoesAwayWhenAppsGoToZero) {
  for (bool software_switch : {true, false}) {
    auto scoped_software_switch_toggler =
        std::make_unique<ScopedCameraMuteToggler>(
            /*software_switch=*/software_switch);
    const std::u16string app_name = u"app_name";
    LaunchAppAccessingCamera(app_name);

    if (IsVideoConferenceEnabled()) {
      // No notification when VC is enabled.
      EXPECT_FALSE(GetSWSwitchNotification());
    } else {
      // No VC - notification to be shown only for SW switch.
      if (software_switch) {
        EXPECT_TRUE(GetSWSwitchNotification());
      } else {
        EXPECT_FALSE(GetSWSwitchNotification());
      }
    }

    // Remove the capturing app, the notification should disappear.
    CloseAppAccessingCamera(app_name);

    EXPECT_FALSE(GetSWSwitchNotification());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyHubCameraSynchronizerTest,
                         /*IsVideoConferenceEnabled=*/testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyHubCameraControllerTest,
                         /*IsVideoConferenceEnabled=*/testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         VideoConferenceCameraControllerTest,
                         /*IsVideoConferenceEnabled=*/testing::Bool());

}  // namespace ash
