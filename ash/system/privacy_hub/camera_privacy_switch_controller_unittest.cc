// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;

namespace ash {

namespace {

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
    auto it = std::find(apps_accessing_camera_.begin(),
                        apps_accessing_camera_.end(), app_name);
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

class RemoveNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  explicit RemoveNotificationWaiter(const std::string& notification_id)
      : notification_id_(notification_id) {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~RemoveNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             const bool by_user) override {
    if (notification_id == notification_id_) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  const std::string notification_id_;
};

}  // namespace

class PrivacyHubCameraControllerTests : public AshTestBase {
 protected:
  PrivacyHubCameraControllerTests()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

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

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto mock_switch = std::make_unique<::testing::NiceMock<MockSwitchAPI>>();
    mock_switch_ = mock_switch.get();

    controller_ = &Shell::Get()->privacy_hub_controller()->camera_controller();
    controller_->SetCameraPrivacySwitchAPIForTest(std::move(mock_switch));
  }

  void LaunchAppAccessingCamera(const std::u16string& app_name) {
    delegate_.LaunchAppAccessingCamera(app_name);
    controller_->ActiveApplicationsChanged(/*application_added=*/true);
  }

  void CloseAppAccessingCamera(const std::u16string& app_name) {
    delegate_.CloseAppAccessingCamera(app_name);
    controller_->ActiveApplicationsChanged(/*application_added=*/false);
  }

  void WaitUntilNotificationRemoved(const std::string& notification_id) {
    RemoveNotificationWaiter waiter{notification_id};
    waiter.Wait();
  }

  ::testing::NiceMock<MockSwitchAPI>* mock_switch_;
  CameraPrivacySwitchController* controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const base::HistogramTester histogram_tester_;
  FakeSensorDisabledNotificationDelegate delegate_;
};

// Test reaction on UI action.
TEST_F(PrivacyHubCameraControllerTests, UIAction) {
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

TEST_F(PrivacyHubCameraControllerTests, OnCameraSoftwarePrivacySwitchChanged) {
  // When |prefs::kUserCameraAllowed| is true and CrOS Camera Service
  // communicates the SW privacy switch state as UNKNOWN or ON, the states
  // mismatch and SetCameraSWPrivacySwitch(kEnabled) should be called to correct
  // the mismatch.
  EXPECT_CALL(*mock_switch_,
              SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting::kEnabled))
      .Times(::testing::Exactly(3));
  SetUserPref(true);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::UNKNOWN);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);

  // When |prefs::kUserCameraAllowed| is false and CrOS Camera Service
  // communicates the SW privacy switch state as UNKNOWN or OFF, the states
  // mismatch and SetCameraSWPrivacySwitch(kDisabled) should be called to
  // correct the mismatch.
  EXPECT_CALL(*mock_switch_,
              SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting::kDisabled))
      .Times(::testing::Exactly(3));
  SetUserPref(false);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::UNKNOWN);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);

  // When the SW privacy switch states match in Privacy Hub and CrOS Camera
  // Service, SetCameraSWPrivacySwitch() should not be called.
  EXPECT_CALL(*mock_switch_, SetCameraSWPrivacySwitch(_))
      .Times(::testing::Exactly(2));

  // When |prefs::kUserCameraAllowed| is true and CrOS Camera Service
  // communicates the SW privacy switch state as OFF, the states match and
  // SetCameraSWPrivacySwitch() should not be called.
  SetUserPref(true);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::OFF);

  // When |prefs::kUserCameraAllowed| is false and CrOS Camera Service
  // communicates the SW privacy switch state as ON, the states match and
  // SetCameraSWPrivacySwitch() should not be called.
  SetUserPref(false);
  controller_->OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState::ON);
}

TEST_F(PrivacyHubCameraControllerTests,
       OnCameraHardwarePrivacySwitchChangedMultipleCameras) {
  CameraPrivacySwitchController& controller =
      Shell::Get()->privacy_hub_controller()->camera_controller();
  // We have 2 cameras in the system.
  controller.OnCameraCountChanged(2);
  // Camera is enabled in Privacy Hub.
  SetUserPref(true);

  // Somebody switched the camera off by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::OFF);
  // Controller must know about it.
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::OFF,
            controller.HWSwitchState());
  EXPECT_FALSE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));

  // Somebody switched the camera off by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::ON);
  // Controller must know about it.
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::ON,
            controller.HWSwitchState());

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  // This particular notification ("Do you want to disable all cameras?") should
  // appear only there are multiple cameras.
  EXPECT_TRUE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
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
  // Click on the notification button.
  message_center->ClickOnNotificationButton(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId, 0);
  // This must change the user pref for the camera (disabling all cameras).
  EXPECT_FALSE(GetUserPref());
  // The notification should be cleared after it has been clicked on.
  EXPECT_FALSE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
  // The histograms were updated.
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
}

TEST_F(PrivacyHubCameraControllerTests,
       OnCameraHardwarePrivacySwitchChangedOneCamera) {
  CameraPrivacySwitchController& controller =
      Shell::Get()->privacy_hub_controller()->camera_controller();
  // We have 1 camera in the system.
  controller.OnCameraCountChanged(1);
  // Camera is enabled in Privacy Hub.
  SetUserPref(true);

  // Somebody switched the camera off by the hardware switch.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::OFF);
  // Controller must know about it.
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::OFF,
            controller.HWSwitchState());
  // This particular notification should appear only if there are multiple
  // cameras.
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));

  // Switching the hardware switch back again.
  controller.OnCameraHWPrivacySwitchStateChanged(
      std::string(), cros::mojom::CameraPrivacySwitchState::ON);
  // Controller is aware.
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::ON,
            controller.HWSwitchState());
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
TEST_F(PrivacyHubCameraControllerTests,
       OnCameraHardwarePrivacySwitchChangedNotificationClearing) {
  CameraPrivacySwitchController& controller =
      Shell::Get()->privacy_hub_controller()->camera_controller();
  SetUserPref(true);
  controller.OnCameraCountChanged(2);

  controller.OnCameraHWPrivacySwitchStateChanged(
      "0", cros::mojom::CameraPrivacySwitchState::ON);
  const message_center::Notification* const notification = FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId);
  EXPECT_TRUE(notification);
  // User should be able to clear the notification manually
  EXPECT_FALSE(notification->rich_notification_data().pinned);
  // Notification should be cleared when hardware mute is disabled
  controller.OnCameraHWPrivacySwitchStateChanged(
      "0", cros::mojom::CameraPrivacySwitchState::OFF);
  WaitUntilNotificationRemoved(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId);
  EXPECT_FALSE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
}

TEST_F(PrivacyHubCameraControllerTests,
       CameraOffNotificationRemoveViaClickOnButton) {
  SetUserPref(false);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  // A notification should be fired.
  EXPECT_TRUE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  // Enabling camera via clicking on the button should clear the notification
  message_center->ClickOnNotificationButton(kPrivacyHubCameraOffNotificationId,
                                            0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
}

TEST_F(PrivacyHubCameraControllerTests,
       CameraOffNotificationRemoveViaClickOnBody) {
  SetUserPref(false);
  controller_->OnCameraCountChanged(2);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  // A notification should be fired.
  EXPECT_TRUE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            0);

  // Enabling camera via clicking on the body should open the privacy hub
  // settings page.
  message_center->ClickOnNotification(kPrivacyHubCameraOffNotificationId);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  // The user pref should not be changed.
  EXPECT_FALSE(GetUserPref());
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);

  SetUserPref(true);

  ASSERT_FALSE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));

  // Flip the hardware switch.
  Shell::Get()
      ->privacy_hub_controller()
      ->camera_controller()
      .OnCameraHWPrivacySwitchStateChanged(
          "0", cros::mojom::CameraPrivacySwitchState::ON);

  // A notification should be fired.
  EXPECT_TRUE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
  EXPECT_TRUE(GetUserPref());

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);

  // Clicking on the body should open the privacy hub settings page.
  message_center->ClickOnNotification(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 2);
  // The user pref should not be changed.
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            2);
}

TEST_F(PrivacyHubCameraControllerTests,
       CameraOffNotificationRemoveViaUserPref) {
  SetUserPref(false);
  ASSERT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  // A notification should be fired.
  EXPECT_TRUE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  // Enabling camera via the user pref should clear the notification
  SetUserPref(true);
  EXPECT_TRUE(GetUserPref());
  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
}

TEST_F(PrivacyHubCameraControllerTests, InSessionSwitchNotification) {
  SetUserPref(true);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  message_center->RemoveNotification(kPrivacyHubCameraOffNotificationId, false);

  // An application starts accessing the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);
  // Disable camera
  SetUserPref(false);

  // A notification should be fired.
  EXPECT_TRUE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  // Enabling camera via clicking on the button should clear the notification
  message_center->ClickOnNotificationButton(kPrivacyHubCameraOffNotificationId,
                                            0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
}

// Tests if the notification `kPrivacyHubCameraOffNotificationId` is removed
// when the number of apps accessing the camera becomes 0.
TEST_F(PrivacyHubCameraControllerTests,
       NotificationRemovedWhenNoActiveApplication) {
  SetUserPref(true);

  // The notification should not be in the message center initially.
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // This is the effect of an application starting to access the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/true);

  // Disabling camera using the software switch.
  SetUserPref(false);

  // Notification `kPrivacyHubCameraOffNotificationId` should pop up.
  EXPECT_TRUE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // The only active application stops accessing the camera the camera.
  controller_->ActiveApplicationsChanged(/*application_added=*/false);

  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

  // Existing notification `kPrivacyHubCameraOffNotificationId` should be
  // removed as the number of active applications is 0 now.
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));
}

// Tests if the camera software switch notification contains proper text.
TEST_F(PrivacyHubCameraControllerTests, NotificationText) {
  // Disabling camera using the software switch.
  SetUserPref(false);
  EXPECT_FALSE(FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // Launch app1 that's accessing camera, a notification should be displayed
  // with the application name in the notification body.
  const std::u16string app1 = u"app1";
  LaunchAppAccessingCamera(app1);

  message_center::Notification* notification =
      FindNotificationById(kPrivacyHubCameraOffNotificationId);
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

  notification = FindNotificationById(kPrivacyHubCameraOffNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app2, app1),
      notification->message());

  // Launch app3 that's also accessing camera, a notification should be
  // displayed again with generic text.
  const std::u16string app3 = u"app3";
  LaunchAppAccessingCamera(app3);

  notification = FindNotificationById(kPrivacyHubCameraOffNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE),
            notification->message());

  // Close one of the applications. The notification should be updated to
  // contain the name of the two remaining applications.
  CloseAppAccessingCamera(app2);

  notification = FindNotificationById(kPrivacyHubCameraOffNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app3, app1),
      notification->message());
}

TEST_F(PrivacyHubCameraControllerTests, MetricCollection) {
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

  CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
      false);
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

  CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
      true);
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

}  // namespace ash
