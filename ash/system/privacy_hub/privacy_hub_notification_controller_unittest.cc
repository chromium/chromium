// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace {

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    return {};
  }
};

}  // namespace

using Sensor = PrivacyHubNotificationController::Sensor;

class PrivacyHubNotificationControllerTest : public AshTestBase {
 public:
  PrivacyHubNotificationControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kCrosPrivacyHubV2);
  }

  ~PrivacyHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    microphone_mute_controller_ =
        Shell::Get()->system_notification_controller()->microphone_mute_.get();
    controller_ =
        Shell::Get()->system_notification_controller()->privacy_hub_.get();
  }
  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  const message_center::Notification* GetNotification(
      const char* notification_id =
          PrivacyHubNotificationController::kCombinedNotificationId) const {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (const auto* notification : notifications) {
      if (notification->id() == notification_id) {
        return notification;
      }
    }
    return nullptr;
  }

  void ClickOnNotificationButton() const {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        PrivacyHubNotificationController::kCombinedNotificationId,
        /*button_index=*/0);
  }

  void ClickOnNotificationBody() const {
    message_center::MessageCenter::Get()->ClickOnNotification(
        PrivacyHubNotificationController::kCombinedNotificationId);
  }

  void ShowNotification(Sensor sensor) {
    if (sensor == Sensor::kMicrophone) {
      microphone_mute_controller()->OnInputMuteChanged(
          true, CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 1}});
    } else {
      controller_->ShowSensorDisabledNotification(sensor);
    }
  }

  void RemoveNotification(Sensor sensor) {
    if (sensor == Sensor::kMicrophone) {
      microphone_mute_controller()->OnInputMuteChanged(
          false, CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 0}});
    } else {
      controller_->RemoveSensorDisabledNotification(sensor);
    }
  }

  void ShowCombinedNotification() {
    ShowNotification(Sensor::kCamera);
    controller_->ShowSensorDisabledNotification(Sensor::kMicrophone);
  }

  MicrophoneMuteNotificationController* microphone_mute_controller() const {
    return microphone_mute_controller_;
  }

  void ExpectNoNotificationActive() const {
    EXPECT_FALSE(GetNotification());
    EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
    EXPECT_FALSE(
        GetNotification(MicrophoneMuteNotificationController::kNotificationId));
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::raw_ptr<PrivacyHubNotificationController> controller_;
  const FakeSensorDisabledNotificationDelegate delegate_;
  const base::HistogramTester histogram_tester_;
  base::raw_ptr<MicrophoneMuteNotificationController>
      microphone_mute_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyHubNotificationControllerTest, ShowCameraNotification) {
  ExpectNoNotificationActive();
  ShowNotification(Sensor::kCamera);
  EXPECT_TRUE(GetNotification(kPrivacyHubCameraOffNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, ShowMicrophoneNotification) {
  ExpectNoNotificationActive();

  ShowNotification(Sensor::kMicrophone);
  EXPECT_TRUE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));

  RemoveNotification(Sensor::kMicrophone);
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationActive) {
  ExpectNoNotificationActive();

  ShowCombinedNotification();
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationBuilding) {
  ExpectNoNotificationActive();

  ShowNotification(Sensor::kMicrophone);
  EXPECT_FALSE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_TRUE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));

  ShowNotification(Sensor::kCamera);
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));

  // Enable microphone from elsewhere.
  RemoveNotification(Sensor::kMicrophone);
  EXPECT_FALSE(GetNotification());
  EXPECT_TRUE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));

  // Remove the camera notification as well.
  RemoveNotification(Sensor::kCamera);
  ExpectNoNotificationActive();
}

TEST_F(PrivacyHubNotificationControllerTest,
       CombinedNotificationClickedButOnlyOneSensorEnabledInSettings) {
  ExpectNoNotificationActive();

  ShowCombinedNotification();
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            0);

  ClickOnNotificationBody();

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);
  ExpectNoNotificationActive();

  // Go to (quick)settings and enable microphone.
  RemoveNotification(Sensor::kMicrophone);

  // Since the user clicked on the notification body they acknowledged that
  // camera is disabled as well. So don't show that notification even though
  // the sensor is still disabled.
  ExpectNoNotificationActive();

  // Disable camera as well
  RemoveNotification(Sensor::kCamera);
  ExpectNoNotificationActive();

  // Now that no sensor is in use anymore when accessing both again the
  // combined notification should show up again.
  ShowCombinedNotification();
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophoneMuteNotificationController::kNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationButton) {
  ExpectNoNotificationActive();
  ShowCombinedNotification();
  EXPECT_TRUE(GetNotification());

  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                true),
            0);

  ClickOnNotificationButton();

  ExpectNoNotificationActive();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubCameraEnabledFromNotificationHistogram,
                true),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                true),
            1);
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationBody) {
  ExpectNoNotificationActive();
  ShowCombinedNotification();
  EXPECT_TRUE(GetNotification());

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            0);

  ClickOnNotificationBody();

  ExpectNoNotificationActive();
  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);
}

TEST_F(PrivacyHubNotificationControllerTest, OpenPrivacyHubSettingsPage) {
  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            0);

  PrivacyHubNotificationController::OpenPrivacyHubSettingsPage();

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                privacy_hub_metrics::kPrivacyHubOpenedHistogram,
                privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification),
            1);
}

}  // namespace ash
