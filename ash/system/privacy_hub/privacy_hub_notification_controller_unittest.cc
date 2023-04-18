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
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
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
    controller_ = PrivacyHubNotificationController::Get();
  }
  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  const message_center::Notification* GetNotification() const {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (const auto* notification : notifications) {
      if (notification->id() ==
          PrivacyHubNotificationController::kCombinedNotificationId) {
        return notification;
      }
    }
    return nullptr;
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
      Shell::Get()
          ->privacy_hub_controller()
          ->microphone_controller()
          .OnInputMuteChanged(true,
                              CrasAudioHandler::InputMuteChangeMethod::kOther);
      FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
          {{"CRAS_CLIENT_TYPE_CHROME", 1}});
    } else {
      controller_->ShowSoftwareSwitchNotification(sensor);
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

  MockNewWindowDelegate* new_window_delegate() { return new_window_delegate_; }

 private:
  base::raw_ptr<PrivacyHubNotificationController> controller_;
  const FakeSensorDisabledNotificationDelegate delegate_;
  const base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::raw_ptr<MockNewWindowDelegate> new_window_delegate_ = nullptr;
  std::unique_ptr<ash::TestNewWindowDelegateProvider> window_delegate_provider_;
};

TEST_F(PrivacyHubNotificationControllerTest, CameraNotificationShowAndHide) {
  EXPECT_FALSE(GetNotification());

  ShowNotification(Sensor::kCamera);

  const message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE),
      notification_ptr->title());

  RemoveNotification(Sensor::kCamera);

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       MicrophoneNotificationShowAndHide) {
  EXPECT_FALSE(GetNotification());

  ShowNotification(Sensor::kMicrophone);

  const message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
            notification_ptr->title());

  RemoveNotification(Sensor::kMicrophone);

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationShowAndHide) {
  EXPECT_FALSE(GetNotification());

  ShowCombinedNotification();

  const message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());

  RemoveCombinedNotification();

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationControllerTest, CombinedNotificationBuilding) {
  EXPECT_FALSE(GetNotification());

  ShowNotification(Sensor::kMicrophone);

  const message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE),
            notification_ptr->title());

  ShowNotification(Sensor::kCamera);

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());

  RemoveNotification(Sensor::kMicrophone);

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE),
      notification_ptr->title());

  RemoveNotification(Sensor::kCamera);

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationControllerTest,
       CombinedNotificationClickedButOnlyOneSensorEnabledInSettings) {
  EXPECT_FALSE(GetNotification());

  ShowCombinedNotification();

  const message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());

  ClickOnNotificationBody();

  EXPECT_FALSE(GetNotification());

  // Go to (quick)settings and enable microphone.
  RemoveNotification(Sensor::kMicrophone);

  // Since the user clicked on the notification body they acknowledged that
  // camera is disabled as well. So don't show that notification even though
  // the sensor is still disabled.
  EXPECT_FALSE(GetNotification());

  // Disable camera as well
  RemoveNotification(Sensor::kCamera);
  EXPECT_FALSE(GetNotification());

  // Now that no sensor is in use anymore when accessing both again the
  // combined notification should show up again.
  ShowCombinedNotification();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
            notification_ptr->title());
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationButton) {
  EXPECT_FALSE(GetNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetNotification());
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubCameraEnabledFromNotificationHistogram,
                   true));
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubMicrophoneEnabledFromNotificationHistogram,
                   true));

  ClickOnNotificationButton();

  EXPECT_FALSE(GetNotification());
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
  EXPECT_FALSE(GetNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetNotification());

  EXPECT_EQ(
      0, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));
  EXPECT_EQ(0, GetSystemTrayClient()->show_os_settings_privacy_hub_count());

  ClickOnNotificationButton(1);

  EXPECT_FALSE(GetNotification());

  EXPECT_EQ(1, GetSystemTrayClient()->show_os_settings_privacy_hub_count());
  EXPECT_EQ(
      1, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));
}

TEST_F(PrivacyHubNotificationControllerTest, ClickOnNotificationBody) {
  EXPECT_FALSE(GetNotification());

  ShowCombinedNotification();

  EXPECT_TRUE(GetNotification());
  EXPECT_EQ(
      0, histogram_tester().GetBucketCount(
             privacy_hub_metrics::kPrivacyHubOpenedHistogram,
             privacy_hub_metrics::PrivacyHubNavigationOrigin::kNotification));

  ClickOnNotificationBody();

  EXPECT_FALSE(GetNotification());
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

  EXPECT_CALL(*new_window_delegate(), OpenUrl).Times(2);

  test_sensor(Sensor::kMicrophone, PrivacyHubLearnMoreSensor::kMicrophone);
  test_sensor(Sensor::kCamera, PrivacyHubLearnMoreSensor::kCamera);
  test_sensor(Sensor::kLocation, PrivacyHubLearnMoreSensor::kGeolocation);
}

}  // namespace ash
