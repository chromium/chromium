// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_notify_notification_blocker.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
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

// Add a notification to the message center. An empty notifier title creates a
// system notification.
void AddNotification(const std::string& notification_id,
                     const std::u16string& notifier_title) {
  const message_center::NotifierId notifier_id =
      notifier_title.empty()
          ? message_center::NotifierId(
                message_center::NotifierType::SYSTEM_COMPONENT, "system")
          : message_center::NotifierId(/*url=*/GURL(), notifier_title);

  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          u"test-title", u"test-message", /*icon=*/gfx::Image(),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          notifier_id, message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>()));
}

// Removes the notification with the given ID.
void RemoveNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*by_user=*/true);
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

// Returns true if the HPS notify informational popup is popped-up.
bool InfoPopupVisible() {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(
             HpsNotifyNotificationBlocker::kInfoNotificationId) != nullptr;
}

// Returns the index at which the given substring appears in the informational
// popup's message, or npos otherwise.
size_t PositionInInfoPopupMessage(const std::u16string& substr) {
  const message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindPopupNotificationById(
          HpsNotifyNotificationBlocker::kInfoNotificationId);
  return notification ? notification->message().find(substr)
                      : std::u16string::npos;
}

// A blocker that blocks only a popup with the given ID.
class IdPopupBlocker : public message_center::NotificationBlocker {
 public:
  IdPopupBlocker(message_center::MessageCenter* message_center)
      : NotificationBlocker(message_center) {}
  IdPopupBlocker(const IdPopupBlocker&) = delete;
  IdPopupBlocker& operator=(const IdPopupBlocker&) = delete;
  ~IdPopupBlocker() override = default;

  void SetTargetId(const std::string& target_id) {
    target_id_ = target_id;
    NotifyBlockingStateChanged();
  }

  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return notification.id() != target_id_;
  }

 private:
  std::string target_id_;
};

// A test fixture that gives access to the HPS notify controller (to fake
// snooping events).
class HpsNotifyNotificationBlockerTest : public AshTestBase {
 public:
  HpsNotifyNotificationBlockerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures({ash::features::kSnoopingProtection},
                                          {ash::features::kQuickDim});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kHasHps);
  }

  HpsNotifyNotificationBlockerTest(const HpsNotifyNotificationBlockerTest&) =
      delete;
  HpsNotifyNotificationBlockerTest& operator=(
      const HpsNotifyNotificationBlockerTest&) = delete;

  ~HpsNotifyNotificationBlockerTest() override = default;

  // AshTestBase overrides:
  void SetUp() override {
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
    message_center_ = message_center::MessageCenter::Get();
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble();
  }

  bool HasHpsNotification() {
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            HpsNotifyNotificationBlocker::kInfoNotificationId);
    return notification != nullptr;
  }

  void SimulateClick(int button_index) {
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            HpsNotifyNotificationBlocker::kInfoNotificationId);
    notification->delegate()->Click(button_index, absl::nullopt);
  }

  int GetNumOsSmartPrivacySettingsOpened() {
    return GetSystemTrayClient()->show_os_smart_privacy_settings_count();
  }

 protected:
  HpsNotifyController* controller_ = nullptr;
  message_center::MessageCenter* message_center_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(HpsNotifyNotificationBlockerTest, Snooping) {
  SetBlockerPref(true);

  // By default, no snooper detected.
  AddNotification("notification-1", u"notifier-1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*state=*/hps::HpsResult::POSITIVE);

  // When snooping is detected, the popup notification should be hidden but
  // remain in the notification queue. Note that, since the popup has been
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Add notifications while a snooper is present.
  AddNotification("notification-2", u"notifier-2");
  AddNotification("notification-3", u"notifier-3");
  EXPECT_EQ(VisiblePopupCount(), 1u);  // Only our info popup.
  EXPECT_TRUE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 4u);

  // Simulate snooper absence. We wait for a moment to bypass the controller's
  // hysteresis logic.
  controller_->OnHpsNotifyChanged(/*state=*/hps::HpsResult::NEGATIVE);
  task_environment()->FastForwardBy(base::Seconds(10));

  // The unshown popups should appear since snooper has left.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(HpsNotifyNotificationBlockerTest, DISABLED_Pref) {
  SetBlockerPref(false);

  // Start with one notification that shouldn't be hidden.
  AddNotification("notification-1", u"notifier-1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // Notifications should be visible up until the user enables the feature.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);
  SetBlockerPref(true);

  // The only popup now visible should be our info popup. Note that, since
  // notification-1 has been previously shown, it won't be shown again.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_TRUE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Add a notification while the feature is disabled.
  AddNotification("notification-2", u"notifier-2");
  AddNotification("notification-3", u"notifier-3");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_TRUE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Notifications should be shown if *either* the whole setting or the
  // subfeature is enabled.
  SetSnoopingPref(false);

  // The new popups should appear when the feature is disabled.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(HpsNotifyNotificationBlockerTest, SystemNotification) {
  SetBlockerPref(true);

  // One regular notification, one important notification that should be
  // allowlisted, and one important notification that could contain sensitive
  // information (and should therefore still be blocked).
  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", /*notifier_title=*/u"");
  AddNotification(
      SmsObserver::kNotificationPrefix + std::string("-notification-3"),
      /*notifier_title=*/u"");
  EXPECT_EQ(VisiblePopupCount(), 3u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // The safe notification shouldn't be suppressed, but the sensitive
  // notification should be.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_TRUE(InfoPopupVisible());
  // Regular notification disappears because it was already shown before the
  // snooper arrived.
  EXPECT_EQ(PositionInInfoPopupMessage(u"notifier-1"), std::u16string::npos);
  // Check that the system notification is labled as such.
  EXPECT_NE(PositionInInfoPopupMessage(u"System"), std::u16string::npos);
  EXPECT_EQ(VisibleNotificationCount(), 4u);
}

TEST_F(HpsNotifyNotificationBlockerTest, InfoPopup) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // Two notifications we're blocking.
  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");
  EXPECT_EQ(VisiblePopupCount(), 1u);  // Only our info popup.
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-1"), std::u16string::npos);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Check that the user can remove the info popup and it will return.
  RemoveNotification(HpsNotifyNotificationBlocker::kInfoNotificationId);
  EXPECT_EQ(VisiblePopupCount(), 0u);
  AddNotification("notification-3", u"notifier-3");
  EXPECT_EQ(VisiblePopupCount(), 1u);  // Only our info popup.
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-3"), std::u16string::npos);
}

// Test that we don't report the notifiers of popups that we (alone) aren't
// blocking.
TEST_F(HpsNotifyNotificationBlockerTest, InfoPopupOtherBlocker) {
  IdPopupBlocker other_blocker(message_center_);
  other_blocker.SetTargetId("notification-2");

  SetBlockerPref(true);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // One notification only we are blocking, and one notification that is also
  // blocked by another blocker.
  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-1"), std::u16string::npos);
  // Do not report that we're blocking a notification when it won't show up
  // after snooping ends.
  EXPECT_EQ(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Now update our other blocker not to block either notification.
  other_blocker.SetTargetId("notification-3");

  // We are now the sole blockers of both notifications, so should report both.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-1"), std::u16string::npos);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

// Test that the info popup message is changed as relevant notifications are
// added and removed.
TEST_F(HpsNotifyNotificationBlockerTest, InfoPopupChangingNotifications) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  // Newer notifiers should come before older ones.
  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");
  {
    const size_t pos_1 = PositionInInfoPopupMessage(u"notifier-1");
    const size_t pos_2 = PositionInInfoPopupMessage(u"notifier-2");
    EXPECT_LE(pos_2, pos_1);
    EXPECT_LE(pos_1, std::u16string::npos);
  }

  // Positions should be swapped if we see an old notifier again.
  AddNotification("notification-3", u"notifier-1");
  {
    const size_t pos_1 = PositionInInfoPopupMessage(u"notifier-1");
    const size_t pos_2 = PositionInInfoPopupMessage(u"notifier-2");
    EXPECT_LE(pos_1, pos_2);
    EXPECT_LE(pos_2, std::u16string::npos);
  }

  // Notifiers don't repeat.
  AddNotification("notification-4", u"notifier-1");
  {
    const size_t pos_1 = PositionInInfoPopupMessage(u"notifier-1");
    const size_t pos_2 = PositionInInfoPopupMessage(u"notifier-2");
    EXPECT_LE(pos_1, pos_2);
    EXPECT_LE(pos_2, std::u16string::npos);
  }

  // Notifiers are removed correctly.
  RemoveNotification("notification-4");
  RemoveNotification("notification-2");
  {
    const size_t pos_1 = PositionInInfoPopupMessage(u"notifier-1");
    const size_t pos_2 = PositionInInfoPopupMessage(u"notifier-2");
    EXPECT_NE(pos_1, std::u16string::npos);
    EXPECT_EQ(pos_2, std::u16string::npos);
  }
}

// Test that message center is visible when click "Show" button.
TEST_F(HpsNotifyNotificationBlockerTest, ShowButtonClicked) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");

  EXPECT_TRUE(HasHpsNotification());

  // Click on show button.
  SimulateClick(/*button_index=*/0);
  EXPECT_TRUE(GetMessageCenterBubble()->IsMessageCenterVisible());
}

// Test that message center is visible when click Settings button.
TEST_F(HpsNotifyNotificationBlockerTest, SettingsButtonClicked) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  controller_->OnHpsNotifyChanged(/*snooper=*/hps::HpsResult::POSITIVE);

  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");

  EXPECT_TRUE(HasHpsNotification());

  // Click on show button.
  SimulateClick(/*button_index=*/1);
  EXPECT_EQ(1, GetNumOsSmartPrivacySettingsOpened());
}

}  // namespace

}  // namespace ash
