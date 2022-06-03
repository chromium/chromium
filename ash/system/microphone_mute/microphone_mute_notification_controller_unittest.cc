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
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

class FakeMicrophoneMuteNotificationDelegate
    : public MicrophoneMuteNotificationDelegate {
 public:
  absl::optional<std::u16string> GetAppAccessingMicrophone() override {
    return app_name_;
  }

  void SetAppAccessingMicrophone(
      const absl::optional<std::u16string> app_name) {
    app_name_ = app_name;
  }

  absl::optional<std::u16string> app_name_;
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
    SetMicrophoneMuteSwitchState(/*muted=*/false);
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

  message_center::Notification* GetPopupNotification() {
    const message_center::NotificationList::PopupNotifications& notifications =
        message_center::MessageCenter::Get()->GetPopupNotifications();
    for (auto* notification : notifications) {
      if (notification->id() ==
          MicrophoneMuteNotificationController::kNotificationId) {
        return notification;
      }
    }
    return nullptr;
  }

  void MarkPopupAsShown() {
    message_center::MessageCenter::Get()->MarkSinglePopupAsShown(
        MicrophoneMuteNotificationController::kNotificationId, true);
  }

  void ClickOnNotificationButton() {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        MicrophoneMuteNotificationController::kNotificationId,
        /*button_index=*/0);
  }

  void SetMicrophoneMuteSwitchState(bool muted) {
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(muted);
  }

  void MuteMicrophone() { CrasAudioHandler::Get()->SetInputMute(true); }

  void UnMuteMicrophone() { CrasAudioHandler::Get()->SetInputMute(false); }

  void SetNumberOfActiveInputStreams(int number_of_active_input_streams) {
    chromeos::FakeCrasAudioClient::Get()->SetActiveInputStreamsWithPermission(
        {{"CRAS_CLIENT_TYPE_CHROME", number_of_active_input_streams}});
  }

  void LaunchApp(absl::optional<std::u16string> app_name) {
    delegate_->SetAppAccessingMicrophone(app_name);
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
  LaunchApp(absl::nullopt);
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

  // Launch an app that's using the mic. The microphone mute notification should
  // show as a popup
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  // Unmute again, notification goes down.
  UnMuteMicrophone();
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
       SilentNotificationOnMuteWhileMicInUse) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());

  // Launch an app that's using the mic, no notification because the microphone
  // is not muted.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  EXPECT_FALSE(GetNotification());

  // Mute the mic, a notification should be shown, but not as a popup.
  MuteMicrophone();
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
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

TEST_F(MicrophoneMuteNotificationControllerTest,
       RemovingStreamDoesNotShowPopup) {
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

  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest, MuteNotificationActionButton) {
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have an action button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Clicking the action button should unmute device.
  ClickOnNotificationButton();
  EXPECT_FALSE(chromeos::CrasAudioHandler::Get()->IsInputMuted());

  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
       NoNotificationActionButtonIfMutedByHwSwitch) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should not have an action button if device is muted
  // by a mute switch.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(0u, notification->buttons().size());

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(chromeos::CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
       TogglingMuteSwitchRemovesNotificationActionButton) {
  // Mute microphone, and activate an audio input stream.
  MuteMicrophone();
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);

  // The mute notification should have an action button.
  message_center::Notification* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(1u, notification->buttons().size());

  // Toggle microphone mute switch and verify the action button disappears.
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(0u, notification->buttons().size());

  SetMicrophoneMuteSwitchState(/*muted=*/false);
  ASSERT_FALSE(chromeos::CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
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
  ASSERT_FALSE(chromeos::CrasAudioHandler::Get()->IsInputMuted());
  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
       RemovingAllInputStreamsWhileHwSwitchToggled) {
  SetMicrophoneMuteSwitchState(/*muted=*/true);
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(2);

  EXPECT_TRUE(GetNotification());
  EXPECT_TRUE(GetPopupNotification());

  SetNumberOfActiveInputStreams(0);

  EXPECT_FALSE(GetNotification());
}

TEST_F(MicrophoneMuteNotificationControllerTest,
       ToggleMicrophoneMuteSwitchWhileInputStreamActive) {
  // Launch an app using microphone, and toggle mute switch.
  LaunchApp(u"junior");
  SetNumberOfActiveInputStreams(1);
  SetMicrophoneMuteSwitchState(/*muted=*/true);

  // Notification should be shown, but not as a popup.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

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

}  // namespace ash
