// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace {

class MockNewWindowDelegate
    : public testing::NiceMock<ash::TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

using Sensor = SensorDisabledNotificationDelegate::Sensor;

}  // namespace

class PrivacyHubNotificationControllerTest : public AshTestBase {
 public:
  PrivacyHubNotificationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }

  ~PrivacyHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = PrivacyHubNotificationController::Get();
  }
  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  const message_center::Notification* GetCombinedNotification() const {
    return GetNotification(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }
  const message_center::Notification* GetGeolocationNotification() const {
    return GetNotification(
        PrivacyHubNotificationController::kGeolocationSwitchNotificationId);
  }

  void ClickOnNotificationButton(int button_index = 0) const {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        PrivacyHubNotificationController::kCombinedNotificationId,
        button_index);
  }

  void ClickOnNotificationBody() const {
    message_center::MessageCenter::Get()->ClickOnNotification(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }

  void ShowNotification(Sensor sensor) {
    if (sensor == Sensor::kMicrophone) {
      MicrophonePrivacySwitchController::Get()->OnInputMuteChanged(
          true, CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 1}});
    } else {
      controller_->ShowSoftwareSwitchNotification(sensor);
    }
  }

  void RemoveNotification(Sensor sensor) {
    if (sensor == Sensor::kMicrophone) {
      MicrophonePrivacySwitchController::Get()->OnInputMuteChanged(
          false, CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 0}});
    } else {
      controller_->RemoveSoftwareSwitchNotification(sensor);
    }
  }

  void ShowCombinedNotification() {
    ShowNotification(Sensor::kCamera);
    ShowNotification(Sensor::kMicrophone);
  }

  void RemoveCombinedNotification() {
    RemoveNotification(Sensor::kCamera);
    RemoveNotification(Sensor::kMicrophone);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 private:
  const message_center::Notification* GetNotification(
      const std::string& id) const {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (const message_center::Notification* notification : notifications) {
      if (notification->id() == id) {
        return notification;
      }
    }
    return nullptr;
  }

  raw_ptr<PrivacyHubNotificationController, DanglingUntriaged> controller_;
  const base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(PrivacyHubNotificationControllerTest, CameraNotificationShowAndHide) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowNotification(Sensor::kCamera);

  const message_center::Notification* notification_ptr =
      GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE),
      notification_ptr->title());
  EXPECT_EQ(1u, notification_ptr->buttons().size());

  RemoveNotification(Sensor::kCamera);

  EXPECT_FALSE(GetCombinedNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       MicrophoneNotificationShowAndHide) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowNotification(Sensor::kMicrophone);

  const message_center::Notification* notification_ptr =
      GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
            notification_ptr->title());
  EXPECT_EQ(1u, notification_ptr->buttons().size());

  RemoveNotification(Sensor::kMicrophone);

  EXPECT_FALSE(GetCombinedNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       GeolocationNotificationShowAndHide) {
  EXPECT_FALSE(GetGeolocationNotification());

  ShowNotification(Sensor::kLocation);
  const message_center::Notification* notification_ptr =
      GetGeolocationNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());
  EXPECT_EQ(2u, notification_ptr->buttons().size());

  RemoveNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       GeolocationNotificationThrottling) {
  EXPECT_FALSE(GetGeolocationNotification());

  // t = 0
  // Show and hide the geolocation notification to trigger the throttler.
  ShowNotification(Sensor::kLocation);
  EXPECT_TRUE(GetGeolocationNotification());
  message_center::MessageCenter::Get()->RemoveNotification(
      GetGeolocationNotification()->id(), /*by_user=*/true);
  EXPECT_FALSE(GetGeolocationNotification());

  // Try to show the notification within the first hour, it shouldn't show
  // t = 0
  ShowNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());

  // Try to show it right before the throttler allows the notification to show,
  // it should not show. t = 0:59
  task_environment()->FastForwardBy(base::Minutes(59));
  ShowNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());

  // Try to show the notification after over 1 hour passes, it should not show.
  // t = 1:01
  task_environment()->FastForwardBy(base::Minutes(2));
  ShowNotification(Sensor::kLocation);
  EXPECT_TRUE(GetGeolocationNotification());
  message_center::MessageCenter::Get()->RemoveNotification(
      GetGeolocationNotification()->id(), /*by_user=*/true);
  EXPECT_FALSE(GetGeolocationNotification());

  // Show and remove 1 more time, so that we have three dismissals and hence the
  // 24h throttling kicks in.
  // t = 3:01
  task_environment()->FastForwardBy(base::Hours(2));
  ShowNotification(Sensor::kLocation);
  EXPECT_TRUE(GetGeolocationNotification());
  message_center::MessageCenter::Get()->RemoveNotification(
      GetGeolocationNotification()->id(), /*by_user=*/true);
  EXPECT_FALSE(GetGeolocationNotification());

  // Now the notification should be disabledd until t_0 + 24hours
  // t = 5:01
  task_environment()->FastForwardBy(base::Hours(2));
  ShowNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());
  // t = 7:01
  task_environment()->FastForwardBy(base::Hours(2));
  ShowNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());
  // t = 17:01
  task_environment()->FastForwardBy(base::Hours(10));
  ShowNotification(Sensor::kLocation);
  EXPECT_FALSE(GetGeolocationNotification());

  // After 24 hours the notification should be enabled again
  // t = 24:01
  task_environment()->FastForwardBy(base::Hours(7));
  ShowNotification(Sensor::kLocation);
  EXPECT_TRUE(GetGeolocationNotification());
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationShowAndHide) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowCombinedNotification();

  const message_center::Notification* notification_ptr =
      GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());
  EXPECT_EQ(2u, notification_ptr->buttons().size());

  RemoveCombinedNotification();

  EXPECT_FALSE(GetCombinedNotification());
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationBuilding) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowNotification(Sensor::kMicrophone);

  const message_center::Notification* notification_ptr =
      GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
            notification_ptr->title());

  ShowNotification(Sensor::kCamera);

  notification_ptr = GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());

  RemoveNotification(Sensor::kMicrophone);

  notification_ptr = GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE),
      notification_ptr->title());

  RemoveNotification(Sensor::kCamera);

  EXPECT_FALSE(GetCombinedNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       CombinedNotificationClickedButOnlyOneSensorEnabledInSettings) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowCombinedNotification();

  const message_center::Notification* notification_ptr =
      GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());

  ClickOnNotificationBody();

  EXPECT_FALSE(GetCombinedNotification());

  // Go to (quick)settings and enable microphone.
  RemoveNotification(Sensor::kMicrophone);

  // Since the user clicked on the notification body they acknowledged that
  // camera is disabled as well. So don't show that notification even though
  // the sensor is still disabled.
  EXPECT_FALSE(GetCombinedNotification());

  // Disable camera as well
  RemoveNotification(Sensor::kCamera);
  EXPECT_FALSE(GetCombinedNotification());

  // Now that no sensor is in use anymore when accessing both again the
  // combined notification should show up again.
  ShowCombinedNotification();

  notification_ptr = GetCombinedNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationButton) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetCombinedNotification());
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubCameraEnabledFromNotificationHistogram,
                   true));
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                   true));

  ClickOnNotificationButton();

  EXPECT_FALSE(GetCombinedNotification());
  EXPECT_EQ(1, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubCameraEnabledFromNotificationHistogram,
                   true));
  EXPECT_EQ(1, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                   true));
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnSecondNotificationButton) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetCombinedNotification());

  EXPECT_EQ(
      0, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));
  EXPECT_EQ(0, GetSystemTrayClient()->show_os_settings_privacy_hub_count());

  ClickOnNotificationButton(1);

  EXPECT_FALSE(GetCombinedNotification());

  EXPECT_EQ(1, GetSystemTrayClient()->show_os_settings_privacy_hub_count());
  EXPECT_EQ(
      1, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationBody) {
  EXPECT_FALSE(GetCombinedNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetCombinedNotification());
  EXPECT_EQ(
      0, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));

  ClickOnNotificationBody();

  EXPECT_FALSE(GetCombinedNotification());
}

TEST_F(PrivacyHubNotificationControllerTest, OpenPrivacyHubSettingsPage) {
  EXPECT_EQ(0, GetSystemTrayClient()->show_os_settings_privacy_hub_count());
  EXPECT_EQ(
      0, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));

  PrivacyHubNotificationController::OpenPrivacyHubSettingsPage();

  EXPECT_EQ(1, GetSystemTrayClient()->show_os_settings_privacy_hub_count());
  EXPECT_EQ(
      1, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));
}

TEST_F(PrivacyHubNotificationControllerTest, OpenPrivacyHubSupportPage) {
  using privacy_hub_metrics::PrivacyHubLearnMoreSensor;

  auto test_sensor = [histogram_tester = &histogram_tester()](
                         Sensor privacy_hub_sensor,
                         PrivacyHubLearnMoreSensor lean_more_sensor) {
    EXPECT_EQ(0,
              histogram_tester->GetBucketCount(
                  privacy_hub_metrics::kPrivacyHubLearnMorePageOpenedHistogram,
                  lean_more_sensor));

    PrivacyHubNotificationController::OpenSupportUrl(privacy_hub_sensor);

    EXPECT_EQ(1,
              histogram_tester->GetBucketCount(
                  privacy_hub_metrics::kPrivacyHubLearnMorePageOpenedHistogram,
                  lean_more_sensor));
  };

  EXPECT_CALL(new_window_delegate(), OpenUrl).Times(2);

  test_sensor(Sensor::kMicrophone, PrivacyHubLearnMoreSensor::kMicrophone);
  test_sensor(Sensor::kCamera, PrivacyHubLearnMoreSensor::kCamera);
  test_sensor(Sensor::kLocation, PrivacyHubLearnMoreSensor::kGeolocation);
}

}  // namespace ash
