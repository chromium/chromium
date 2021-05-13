// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

class FakeMicrophoneMuteNotificationDelegate
    : public MicrophoneMuteNotificationDelegate {
 public:
  base::Optional<std::u16string> GetAppAccessingMicrophone() override {
    return app_name_;
  }

  void SetAppAccessingMicrophone(
      const base::Optional<std::u16string> app_name) {
    app_name_ = app_name;
  }

  base::Optional<std::u16string> app_name_;
};

class MicrophoneMuteNotificationControllerTest : public AshTestBase {
 public:
  MicrophoneMuteNotificationControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMicMuteNotifications);
  }
  ~MicrophoneMuteNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<MicrophoneMuteNotificationController>();
    delegate_ = std::make_unique<FakeMicrophoneMuteNotificationDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    delegate_.reset();
    AshTestBase::TearDown();
  }

 protected:
  message_center::Notification* GetNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (auto* notification : notifications) {
      if (notification->id() ==
          MicrophoneMuteNotificationController::kNotificationId) {
        return notification;
      }
    }
    return nullptr;
  }

  void MuteMicrophone() { CrasAudioHandler::Get()->SetInputMute(true); }

  void UnMuteMicrophone() { CrasAudioHandler::Get()->SetInputMute(false); }

  void SetNumberOfActiveInputStreams(int number_of_active_input_streams) {
    chromeos::FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
        {{"CRAS_CLIENT_TYPE_CHROME", number_of_active_input_streams}});
  }

  void LaunchApp(base::Optional<std::u16string> app_name) {
    delegate_->SetAppAccessingMicrophone(app_name);
    controller_->OnNumberOfInputStreamsWithPermissionChanged();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MicrophoneMuteNotificationController> controller_;
  std::unique_ptr<FakeMicrophoneMuteNotificationDelegate> delegate_;
};

TEST_F(MicrophoneMuteNotificationControllerTest, SimpleMuteUnMute) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Or when we mute.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Or when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest, LaunchAppNotUsingMicrophone) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // No notification when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch an app that's not using the mic, should be no notification.
  LaunchApp(base::nullopt);
  SetNumberOfActiveInputStreams(0);
  EXPECT_FALSE(GetNotification());

  // Mute the mic, still no notification because no app is using the mic.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest, LaunchAppUsingMicrophone) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // No notification when we unmute.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Mute the mic, still no notification.
  MuteMicrophone();
  EXPECT_FALSE(GetNotification());

  // Launch an app that's using the mic, should now be a notification.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  EXPECT_TRUE(GetNotification());

  // Unmute again, notification goes down.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

}  // namespace ash
