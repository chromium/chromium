// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/hps_notify_notification_blocker.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Enables or disables the user pref for the entire feature.
void SetSnoopingPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, enabled);
  base::RunLoop().RunUntilIdle();
}

// Enables or disables the user pref for notification blocking.
void SetBlockerPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled, enabled);
  base::RunLoop().RunUntilIdle();
}

// Add a notification to the message center.
void AddNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          u"test_title", u"test message", /*icon=*/gfx::Image(),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     "test app"),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>()));
}

// Returns the number of popup notifications that are currently visible in the
// message center.
size_t VisiblePopupCount() {
  return message_center::MessageCenter::Get()->GetPopupNotifications().size();
}

// Returns the number of (popup or non-popup) notifications that are currently
// visible in the message center queue.
size_t VisibleNotificationCount() {
  return message_center::MessageCenter::Get()->GetVisibleNotifications().size();
}

// A test fixture that gives access to the HPS notify controller (to fake
// snooping events).
class HpsNotifyNotificationBlockerTest : public AshTestBase {
 public:
  HpsNotifyNotificationBlockerTest() = default;

  HpsNotifyNotificationBlockerTest(const HpsNotifyNotificationBlockerTest&) =
      delete;
  HpsNotifyNotificationBlockerTest& operator=(
      const HpsNotifyNotificationBlockerTest&) = delete;

  ~HpsNotifyNotificationBlockerTest() override = default;

  // AshTestBase overrides:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kSnoopingProtection);

    // Simulate a working DBus client.
    chromeos::HpsDBusClient::InitializeFake();
    auto* dbus_client = chromeos::FakeHpsDBusClient::Get();
    dbus_client->set_hps_service_is_available(true);
    dbus_client->set_hps_notify_result(hps::HpsResult::NEGATIVE);

    AshTestBase::SetUp();

    // The controller has now been initialized, part of which entails sending a
    // method to the DBus service. Here we wait for the service to
    // asynchronously respond.
    base::RunLoop().RunUntilIdle();

    // Make sure the controller is active by both logging in and enabling the
    // snooping protection pref.
    SetSnoopingPref(true);

    controller_ = Shell::Get()->hps_notify_controller();
  }

 protected:
  HpsNotifyController* controller_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HpsNotifyNotificationBlockerTest, Snooping) {
  SetBlockerPref(true);

  // By default, no snooper detected.
  AddNotification("notification 1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*state=*/hps::HpsResult::POSITIVE);

  // When snooping is detected, the popup notification should be hidden but
  // remain in the notification queue. Note that, since the popup has been
  // shown, it won't be shown again.
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Add notifications while a snooper is present.
  AddNotification("notification 2");
  AddNotification("notification 3");
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Simulate snooper absence.
  controller_->OnHpsNotifyChanged(/*state=*/hps::HpsResult::NEGATIVE);

  // The unshown popups should appear since snooper has left.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(HpsNotifyNotificationBlockerTest, DISABLED_Pref) {
  SetBlockerPref(false);

  // Start with one notification that shouldn't be hidden.
  AddNotification("notification 1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // Notifications should be visible up until the user enables the feature.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);
  SetBlockerPref(true);

  // Note that, since the popup has been previously shown, it won't be shown
  // again.
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Add a notification while the feature is disabled.
  AddNotification("notification 2");
  AddNotification("notification 3");
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Notifications should be shown if *either* the whole setting or the
  // subfeature is enabled.
  SetSnoopingPref(false);

  // The new popups should appear when the feature is disabled.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(HpsNotifyNotificationBlockerTest, SystemNotification) {
  SetBlockerPref(true);

  // One regular notification, and one important notification that should be
  // allowlisted.
  AddNotification("notification 1");
  AddNotification(BatteryNotification::kNotificationId);
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_EQ(VisibleNotificationCount(), 2u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // The important notification shouldn't be suppressed.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 2u);
}

}  // namespace

}  // namespace ash
