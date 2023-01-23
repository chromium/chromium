// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
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
  const std::string notification_id_;
  base::RunLoop run_loop_;
};

class MockNewWindowDelegate
    : public testing::NiceMock<ash::TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

using Sensor = PrivacyHubNotificationController::Sensor;

class PrivacyHubNotificationControllerTest : public AshTestBase {
 public:
  PrivacyHubNotificationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kCrosPrivacyHubV2);
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    window_delegate_provider_ =
        std::make_unique<ash::TestNewWindowDelegateProvider>(
            std::move(delegate));
  }

  ~PrivacyHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
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
      Shell::Get()
          ->privacy_hub_controller()
          ->microphone_controller()
          .OnInputMuteChanged(true,
                              CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 1}});
    } else {
      controller_->ShowSensorDisabledNotification(sensor);
    }
  }

  void RemoveNotification(Sensor sensor) {
    if (sensor == Sensor::kMicrophone) {
      Shell::Get()
          ->privacy_hub_controller()
          ->microphone_controller()
          .OnInputMuteChanged(false,
                              CrasAudioHandler::InputMuteChangeMethod::kOther);
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

  void ExpectNoNotificationActive() const {
    EXPECT_FALSE(GetNotification());
    EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
    EXPECT_FALSE(
        GetNotification(MicrophonePrivacySwitchController::kNotificationId));
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void WaitUntilNotificationRemoved(const std::string& notification_id) {
    RemoveNotificationWaiter notification_waiter(notification_id);
    notification_waiter.Wait();
  }

  MockNewWindowDelegate* new_window_delegate() { return new_window_delegate_; }

 private:
  base::raw_ptr<PrivacyHubNotificationController> controller_;
  const FakeSensorDisabledNotificationDelegate delegate_;
  const base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::raw_ptr<MockNewWindowDelegate> new_window_delegate_ = nullptr;
  std::unique_ptr<ash::TestNewWindowDelegateProvider> window_delegate_provider_;
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
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));

  RemoveNotification(Sensor::kMicrophone);
  WaitUntilNotificationRemoved(
      MicrophonePrivacySwitchController::kNotificationId);
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationActive) {
  ExpectNoNotificationActive();

  ShowCombinedNotification();
  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationBuilding) {
  ExpectNoNotificationActive();

  ShowNotification(Sensor::kMicrophone);
  EXPECT_FALSE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_TRUE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));

  ShowNotification(Sensor::kCamera);
  WaitUntilNotificationRemoved(
      MicrophonePrivacySwitchController::kNotificationId);
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));

  // Enable microphone from elsewhere.
  RemoveNotification(Sensor::kMicrophone);
  EXPECT_FALSE(GetNotification());
  EXPECT_TRUE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));

  // Remove the camera notification as well.
  RemoveNotification(Sensor::kCamera);
  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

  ExpectNoNotificationActive();
}

TEST_F(PrivacyHubNotificationControllerTest,
       CombinedNotificationClickedButOnlyOneSensorEnabledInSettings) {
  ExpectNoNotificationActive();

  ShowCombinedNotification();
  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));

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
  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetNotification(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(
      GetNotification(MicrophonePrivacySwitchController::kNotificationId));
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

  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

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

  WaitUntilNotificationRemoved(kPrivacyHubCameraOffNotificationId);

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

TEST_F(PrivacyHubNotificationControllerTest, OpenPrivacyHubSupportPage) {
  using privacy_hub_metrics::PrivacyHubLearnMoreSensor;

  auto test_sensor = [histogram_tester = &histogram_tester()](
                         Sensor privacy_hub_sensor,
                         PrivacyHubLearnMoreSensor lean_more_sensor) {
    EXPECT_EQ(histogram_tester->GetBucketCount(
                  privacy_hub_metrics::kPrivacyHubLearnMorePageOpenedHistogram,
                  lean_more_sensor),
              0);

    PrivacyHubNotificationController::OpenSupportUrl(privacy_hub_sensor);

    EXPECT_EQ(histogram_tester->GetBucketCount(
                  privacy_hub_metrics::kPrivacyHubLearnMorePageOpenedHistogram,
                  lean_more_sensor),
              1);
  };

  EXPECT_CALL(*new_window_delegate(), OpenUrl).Times(2);

  test_sensor(Sensor::kMicrophone, PrivacyHubLearnMoreSensor::kMicrophone);
  test_sensor(Sensor::kCamera, PrivacyHubLearnMoreSensor::kCamera);

  if (DCHECK_IS_ON()) {
    EXPECT_DEATH(
        PrivacyHubNotificationController::OpenSupportUrl(Sensor::kLocation),
        "");
  }
}

}  // namespace ash
