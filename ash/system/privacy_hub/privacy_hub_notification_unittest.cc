// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {
namespace {

// static constexpr char kNotificationId[] =
// PrivacyHubNotificationController::kCombinedNotificationId;
static constexpr char kNotificationId[] =
    "ash.system.privacy_hub.enable_microphone_and/or_camera";

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    return apps_;
  }

  void LaunchApp(const std::u16string& app_name) {
    apps_.insert(apps_.begin(), app_name);
  }

  void CloseApp(const std::u16string& app_name) {
    auto it = base::ranges::find(apps_, app_name);
    if (it != apps_.end()) {
      apps_.erase(it);
    }
  }

 private:
  std::vector<std::u16string> apps_;
};

// A waiter class, once `Wait()` is invoked, waits until a pop up of the
// notification with id `kNotificationId` is closed.
class NotificationPopupWaiter : public message_center::MessageCenterObserver {
 public:
  NotificationPopupWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~NotificationPopupWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }
  NotificationPopupWaiter& operator=(const NotificationPopupWaiter&) = delete;
  NotificationPopupWaiter(const NotificationPopupWaiter&) = delete;

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationPopupShown(const std::string& notification_id,
                                bool mark_notification_as_read) override {
    if (notification_id == kNotificationId) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
};

message_center::Notification* GetNotification() {
  return message_center::MessageCenter::Get()->FindNotificationById(
      kNotificationId);
}

message_center::Notification* GetPopupNotification() {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(
      kNotificationId);
}

}  // namespace

class PrivacyHubNotificationTest : public AshTestBase {
 public:
  PrivacyHubNotificationTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PrivacyHubNotificationTest() override = default;

  PrivacyHubNotification& notification() {
    auto& notification = *PrivacyHubNotificationController::Get()
                              ->combined_notification_for_test();
    notification.SetSensors(sensors_);
    return notification;
  }

  FakeSensorDisabledNotificationDelegate& sensor_delegate() {
    return static_cast<FakeSensorDisabledNotificationDelegate&>(
        *PrivacyHubNotificationController::Get()
             ->sensor_disabled_notification_delegate());
  }

  // testing::Test
  void SetUp() override {
    AshTestBase::SetUp();
    // Set up the fake SensorDisabledNotificationDelegate.
    scoped_delegate_ =
        std::make_unique<ScopedSensorDisabledNotificationDelegateForTest>(
            std::make_unique<FakeSensorDisabledNotificationDelegate>());
  }
  // testing::Test
  void TearDown() override {
    // We need to destroy the delegate while the Ash still exists.
    scoped_delegate_.reset();

    AshTestBase::TearDown();
  }

  void WaitUntilPopupCloses() {
    NotificationPopupWaiter waiter;
    waiter.Wait();
  }

 protected:
  SensorDisabledNotificationDelegate::SensorSet sensors_{
      SensorDisabledNotificationDelegate::Sensor::kMicrophone};

 private:
  std::unique_ptr<ScopedSensorDisabledNotificationDelegateForTest>
      scoped_delegate_;
};

enum class NotificationType { CAMERA, MICROPHONE, CAMERA_MICROPHONE };

class PrivacyHubNotificationTextTest
    : public PrivacyHubNotificationTest,
      public testing::WithParamInterface<std::tuple<bool, NotificationType>> {
 public:
  PrivacyHubNotificationTextTest() {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
    scoped_camera_led_fallback_ = std::make_unique<ScopedLedFallbackForTesting>(
        IsCameraLedFallbackActive());
    sensors_ = [this]() -> SensorDisabledNotificationDelegate::SensorSet {
      switch (std::get<1>(this->GetParam())) {
        case NotificationType::CAMERA: {
          return {SensorDisabledNotificationDelegate::Sensor::kCamera};
        }
        case NotificationType::MICROPHONE: {
          return {SensorDisabledNotificationDelegate::Sensor::kMicrophone};
        }
        case NotificationType::CAMERA_MICROPHONE: {
          return {SensorDisabledNotificationDelegate::Sensor::kCamera,
                  SensorDisabledNotificationDelegate::Sensor::kMicrophone};
        }
      }
      NOTREACHED();
    }();
  }

  void SetUp() override {
    PrivacyHubNotificationTest::SetUp();
    IsCameraLedFallbackActive();
  }
  void TearDown() override {
    scoped_camera_led_fallback_.reset();
    PrivacyHubNotificationTest::TearDown();
  }

  bool IsCameraLedFallbackActive() const { return std::get<0>(GetParam()); }

  std::u16string ExpectedText(
      std::initializer_list<std::u16string> app_names = {}) {
    CHECK_LE(app_names.size(), 2ULL);
    CHECK_GT(sensors_.size(), 0ULL);
    CHECK_LE(sensors_.size(), 2ULL);
    const bool microphone =
        sensors_.Has(SensorDisabledNotificationDelegate::Sensor::kMicrophone);
    const bool camera =
        sensors_.Has(SensorDisabledNotificationDelegate::Sensor::kCamera);
    const bool with_disclaimer = IsCameraLedFallbackActive();
    CHECK(microphone || camera);

    const std::array<int, 3> message_ids = [=]() -> std::array<int, 3> {
      if (microphone && camera && with_disclaimer) {
        return {
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_DISCLAIMER,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME_WITH_DISCLAIMER,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES_WITH_DISCLAIMER};
      }
      if (microphone && camera && !with_disclaimer) {
        return {
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};
      }
      if (microphone && !camera) {
        return {IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
                IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
                IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};
      }
      if (!microphone && camera && with_disclaimer) {
        return {
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_DISCLAIMER,
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME_WITH_DISCLAIMER,
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES_WITH_DISCLAIMER};
      }
      if (!microphone && camera && !with_disclaimer) {
        return {
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE,
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
            IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};
      }
      NOTREACHED();
    }();

    const int max_size = 150;
    const std::u16string text = l10n_util::GetStringFUTF16(
        message_ids[app_names.size()], app_names, nullptr);
    if (text.size() <= max_size) {
      return text;
    }
    return l10n_util::GetStringUTF16(message_ids[0]);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScopedLedFallbackForTesting> scoped_camera_led_fallback_;
};

using PrivacyHubNotificationClickDelegateTest = AshTestBase;

TEST_F(PrivacyHubNotificationClickDelegateTest, Click) {
  size_t button_clicked = 0;
  size_t message_clicked = 0;
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate =
      base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
          base::BindLambdaForTesting(
              [&button_clicked]() { button_clicked++; }));

  // Clicking the message while no callback for it is added shouldn't result in
  // a callback being executed.
  delegate->Click(std::nullopt, std::nullopt);

  EXPECT_EQ(button_clicked, 0u);
  EXPECT_EQ(message_clicked, 0u);

  // Click the button.
  delegate->Click(0, std::nullopt);

  EXPECT_EQ(button_clicked, 1u);
  EXPECT_EQ(message_clicked, 0u);

  // Add a message callback.
  delegate->SetMessageClickCallback(
      base::BindLambdaForTesting([&message_clicked]() { message_clicked++; }));

  // When clicking the button, only the button callback should be executed.
  delegate->Click(0, std::nullopt);

  EXPECT_EQ(button_clicked, 2u);
  EXPECT_EQ(message_clicked, 0u);

  // Clicking the message should execute the message callback.
  delegate->Click(std::nullopt, std::nullopt);

  EXPECT_EQ(button_clicked, 2u);
  EXPECT_EQ(message_clicked, 1u);
}

TEST(PrivacyHubNotificationClickDelegateDeathTest, AddButton) {
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate =
      base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
          base::DoNothing());

  // There is no valid callback for the first button. This should only fail on
  // debug builds, in release builds this will simply not run the callback.
  EXPECT_DCHECK_DEATH(delegate->Click(1, std::nullopt));

  // There is no second button, this could lead to out of bounds issues.
  EXPECT_CHECK_DEATH(delegate->Click(2, std::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacyHubNotificationTextTest,
    testing::Combine(testing::Bool(),
                     testing::Values(NotificationType::CAMERA,
                                     NotificationType::MICROPHONE,
                                     NotificationType::CAMERA_MICROPHONE)));

TEST_F(PrivacyHubNotificationTest, ShowAndHide) {
  EXPECT_FALSE(GetNotification());

  notification().Show();

  EXPECT_TRUE(GetNotification());

  notification().Hide();

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationTest, ShowMultipleTimes) {
  EXPECT_FALSE(GetNotification());

  notification().Show();

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  WaitUntilPopupCloses();

  // The notification pop up should close by now. But the notification should
  // stay in the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  notification().Show();

  // The notification should pop up again after `Show()` is called.
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  WaitUntilPopupCloses();

  // The notification pop up should close by now. But the notification should
  // stay in the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());
}

TEST_F(PrivacyHubNotificationTest, UpdateNotification) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  notification().Show();
  // The notification should pop up.
  EXPECT_TRUE(GetPopupNotification());

  // Wait until pop up of the notification is closed.
  WaitUntilPopupCloses();
  // The notification pop up should close by now. But the notification should
  // stay in the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  notification().Update();
  // The update should be silent. The notification should not pop up but stay in
  // the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());
}

TEST_P(PrivacyHubNotificationTextTest, WithApps) {
  // No apps -> generic notification text.
  notification().Show();

  message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(notification_ptr->message(), ExpectedText());

  // Launch a single app -> notification with message for one app.
  const std::u16string app1 = u"test1";
  sensor_delegate().LaunchApp(app1);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(notification_ptr->message(), ExpectedText({app1}));

  // Launch a second app -> notification with message for two apps.
  const std::u16string app2 = u"test2";
  sensor_delegate().LaunchApp(app2);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(ExpectedText({app1, app2}), notification_ptr->message());

  // More than two apps -> generic notification text.
  const std::u16string app3 = u"test3";
  sensor_delegate().LaunchApp(app3);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(ExpectedText(), notification_ptr->message());

  // Close one of the applications -> notification with message for two apps.
  sensor_delegate().CloseApp(app2);
  notification().Update();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(ExpectedText({app1, app3}), notification_ptr->message());
}

TEST_P(PrivacyHubNotificationTextTest, NotificationMessageForLongAppNames) {
  const std::u16string short_app_name = u"A";
  const std::u16string long_app_name =
      u"SomeAppWithAReallyReallyReallyReallyReallyReallyReallyReallyReallyReall"
      u"yReallyReallyReallyReallyReallyReallyReallyReallyReallyLongName";
  sensor_delegate().LaunchApp(short_app_name);
  notification().Show();

  message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  const std::u16string first_message = notification_ptr->message();
  EXPECT_LE(first_message.size(), 150u);

  sensor_delegate().CloseApp(short_app_name);

  // Generate a notification that should now exceed the max length.
  sensor_delegate().LaunchApp(long_app_name);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  // The new notification should also be at most 150 characters long.
  EXPECT_LE(notification_ptr->message().size(), 150u);
  // If the camera led is not active, it shouldn't be identical to the old
  // message even with the same length.
  if (!IsCameraLedFallbackActive()) {
    EXPECT_NE(first_message, notification_ptr->message());
  }
}

class PrivacyHubNotificationForScreenCaptureWithMicrophone
    : public PrivacyHubNotificationTextTest {};

TEST_P(PrivacyHubNotificationForScreenCaptureWithMicrophone, Test) {
  // Launch an app.
  const std::u16string app_1 = u"App1";
  sensor_delegate().LaunchApp(app_1);
  notification().Show();

  // Shall be a notification with 1 app name.
  message_center::Notification* privacy_hub_notification = GetNotification();
  ASSERT_TRUE(privacy_hub_notification);
  EXPECT_EQ(privacy_hub_notification->message(), ExpectedText({app_1}));

  // Start screen capture with audio from microphone.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  controller->StartVideoRecordingImmediatelyForTesting();
  notification().Update();

  // Shall be a notification with 2 app names.
  const std::u16string screenCaptureTitle =
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE);
  privacy_hub_notification = GetNotification();
  ASSERT_TRUE(privacy_hub_notification);
  EXPECT_EQ(ExpectedText({app_1, screenCaptureTitle}),
            privacy_hub_notification->message());

  // Stop screen capture.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  notification().Update();

  // Shall be a notification with 1 app name.
  privacy_hub_notification = GetNotification();
  ASSERT_TRUE(privacy_hub_notification);
  EXPECT_EQ(ExpectedText({app_1}), privacy_hub_notification->message());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacyHubNotificationForScreenCaptureWithMicrophone,
    testing::Combine(testing::Values(false, true),
                     testing::Values(NotificationType::MICROPHONE)));

}  // namespace ash
