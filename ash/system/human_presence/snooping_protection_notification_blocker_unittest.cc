// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_notification_blocker.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/human_presence/snooping_protection_notification_blocker_internal.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/dbus/human_presence/fake_human_presence_dbus_client.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/base/l10n/l10n_util.h"
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
                message_center::NotifierType::SYSTEM_COMPONENT, "system",
                NotificationCatalogName::kHPSNotify)
          : message_center::NotifierId(/*url=*/GURL(), notifier_title,
                                       /*web_app_id=*/std::nullopt);

  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"test-title", u"test-message", /*icon=*/ui::ImageModel(),
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
             SnoopingProtectionNotificationBlocker::kInfoNotificationId) !=
         nullptr;
}

// Returns the index at which the given substring appears in the informational
// popup's message, or npos otherwise.
size_t PositionInInfoPopupMessage(const std::u16string& substr) {
  const message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindPopupNotificationById(
          SnoopingProtectionNotificationBlocker::kInfoNotificationId);
  return notification ? notification->message().find(substr)
                      : std::u16string::npos;
}

// A blocker that blocks only a popup with the given ID.
class IdPopupBlocker : public message_center::NotificationBlocker {
 public:
  explicit IdPopupBlocker(message_center::MessageCenter* message_center)
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

// An app registry cache that can be used to register test app names.
class FakeAppRegistryCache {
 public:
  // The following methods match the interface of apps::AppRegistryCacheWrapper.

  static FakeAppRegistryCache& Get() {
    static base::NoDestructor<FakeAppRegistryCache> kInstance;
    return *kInstance;
  }

  // In the real class, returns the cache for the given user. We only maintain a
  // single fake cache.
  FakeAppRegistryCache* GetAppRegistryCache(const AccountId&) { return this; }

  template <typename FunctionType>
  bool ForOneApp(const std::string& app_id, FunctionType f) {
    for (const std::unique_ptr<apps::AppUpdate>& app : apps_) {
      if (app_id == app->AppId()) {
        f(*app);
        return true;
      }
    }

    return false;
  }

  // Test-only method to populate fake apps.
  void AddApp(std::unique_ptr<apps::AppUpdate> app) {
    apps_.push_back(std::move(app));
  }

 private:
  // Use pointers as we need a default constructor.
  std::vector<std::unique_ptr<apps::AppUpdate>> apps_;
};

// A test fixture that gives access to the HPS notify controller (to fake
// snooping events).
class SnoopingProtectionNotificationBlockerTest : public AshTestBase {
 public:
  SnoopingProtectionNotificationBlockerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {/*enabled_features=*/{
            ash::features::kSnoopingProtection,
            {
                {"SnoopingProtection_pos_window_ms", "4000"},
                {"SnoopingProtection_filter_config_case", "2"},
                {"SnoopingProtection_positive_count_threshold", "1"},
                {"SnoopingProtection_negative_count_threshold", "1"},
                {"SnoopingProtection_uncertain_count_threshold", "1"},
                {"SnoopingProtection_positive_score_threshold", "0"},
                {"SnoopingProtection_negative_score_threshold", "0"},
            }}},
        /*disabled_features=*/{ash::features::kQuickDim});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kHasHps);
  }

  SnoopingProtectionNotificationBlockerTest(
      const SnoopingProtectionNotificationBlockerTest&) = delete;
  SnoopingProtectionNotificationBlockerTest& operator=(
      const SnoopingProtectionNotificationBlockerTest&) = delete;

  ~SnoopingProtectionNotificationBlockerTest() override = default;

  // AshTestBase overrides:
  void SetUp() override {
    // Simulate a working DBus client.
    HumanPresenceDBusClient::InitializeFake();
    auto* dbus_client = FakeHumanPresenceDBusClient::Get();
    dbus_client->set_hps_service_is_available(true);
    hps::HpsResultProto state;
    state.set_value(hps::HpsResult::NEGATIVE);
    dbus_client->set_hps_notify_result(state);

    AshTestBase::SetUp();

    // The controller has now been initialized, part of which entails sending a
    // method to the DBus service. Here we wait for the service to
    // asynchronously respond.
    base::RunLoop().RunUntilIdle();

    // Make sure the controller is active by both logging in and enabling the
    // snooping protection pref.
    SetSnoopingPref(true);

    controller_ = Shell::Get()->snooping_protection_controller();
    message_center_ = message_center::MessageCenter::Get();
  }

  bool HasInfoNotification() {
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            SnoopingProtectionNotificationBlocker::kInfoNotificationId);
    return notification != nullptr;
  }

  void SimulateClick(int button_index) {
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            SnoopingProtectionNotificationBlocker::kInfoNotificationId);
    notification->delegate()->Click(button_index, std::nullopt);
  }

  int GetNumOsSmartPrivacySettingsOpened() {
    return GetSystemTrayClient()->show_os_smart_privacy_settings_count();
  }

 protected:
  raw_ptr<SnoopingProtectionController, DanglingUntriaged> controller_ =
      nullptr;
  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_ =
      nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(SnoopingProtectionNotificationBlockerTest, Snooping) {
  SetBlockerPref(true);

  // By default, no snooper detected.
  AddNotification("notification-1", u"notifier-1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

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
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  task_environment()->FastForwardBy(base::Seconds(10));

  // The unshown popups should appear since snooper has left.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(SnoopingProtectionNotificationBlockerTest, Pref) {
  SetBlockerPref(false);

  // Start with one notification that shouldn't be hidden.
  AddNotification("notification-1", u"notifier-1");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

  // Notifications should be visible up until the user enables the feature.
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 1u);
  SetBlockerPref(true);

  // Since notification 1 has been shown, we aren't blocking any popups and our
  // info popup need not be shown.
  EXPECT_EQ(VisiblePopupCount(), 0u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Add notifications while the feature is disabled. This should cause our info
  // popup and no other popups to be shown.
  AddNotification("notification-2", u"notifier-2");
  AddNotification("notification-3", u"notifier-3");
  EXPECT_EQ(VisiblePopupCount(), 1u);
  EXPECT_TRUE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 4u);

  // Notifications should be shown if *either* the whole setting or the
  // subfeature is enabled.
  SetSnoopingPref(false);

  // The new popups should appear when the feature is disabled.
  EXPECT_EQ(VisiblePopupCount(), 2u);
  EXPECT_FALSE(InfoPopupVisible());
  EXPECT_EQ(VisibleNotificationCount(), 3u);
}

TEST_F(SnoopingProtectionNotificationBlockerTest, SystemNotification) {
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
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

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

TEST_F(SnoopingProtectionNotificationBlockerTest, InfoPopup) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

  // Two notifications we're blocking.
  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");
  EXPECT_EQ(VisiblePopupCount(), 1u);  // Only our info popup.
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-1"), std::u16string::npos);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_EQ(VisibleNotificationCount(), 3u);

  // Check that the user can remove the info popup and it will return.
  RemoveNotification(
      SnoopingProtectionNotificationBlocker::kInfoNotificationId);
  EXPECT_EQ(VisiblePopupCount(), 0u);
  AddNotification("notification-3", u"notifier-3");
  EXPECT_EQ(VisiblePopupCount(), 1u);  // Only our info popup.
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-2"), std::u16string::npos);
  EXPECT_NE(PositionInInfoPopupMessage(u"notifier-3"), std::u16string::npos);
}

// Test that we don't report the notifiers of popups that we (alone) aren't
// blocking.
TEST_F(SnoopingProtectionNotificationBlockerTest, InfoPopupOtherBlocker) {
  IdPopupBlocker other_blocker(message_center_);
  other_blocker.Init();
  other_blocker.SetTargetId("notification-2");

  SetBlockerPref(true);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

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
TEST_F(SnoopingProtectionNotificationBlockerTest,
       InfoPopupChangingNotifications) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

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

// Test that message center is visible when click Settings button.
TEST_F(SnoopingProtectionNotificationBlockerTest, SettingsButtonClicked) {
  SetBlockerPref(true);

  // Simulate snooper presence.
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);

  AddNotification("notification-1", u"notifier-1");
  AddNotification("notification-2", u"notifier-2");

  EXPECT_TRUE(HasInfoNotification());

  // Click on show button.
  SimulateClick(/*button_index=*/1);
  EXPECT_EQ(1, GetNumOsSmartPrivacySettingsOpened());
}

TEST(SnoopingProtectionNotificationBlockerInternalTest, WebsiteNotifierTitles) {
  // Website without title uses a generic "web" string.
  const message_center::NotifierId untrusted_notifier(
      GURL("http://untrusted.com:443"));
  const std::u16string untrusted_title =
      hps_internal::GetNotifierTitle<FakeAppRegistryCache>(untrusted_notifier,
                                                           AccountId());
  EXPECT_EQ(untrusted_title,
            l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_LOWER));

  // Website with a trusted title uses the title.
  const message_center::NotifierId trusted_notifier(
      GURL("https://trusted.com:443"), u"Trusted",
      /*web_app_id=*/std::nullopt);
  const std::u16string trusted_title =
      hps_internal::GetNotifierTitle<FakeAppRegistryCache>(trusted_notifier,
                                                           AccountId());
  EXPECT_EQ(trusted_title, u"Trusted");
}

TEST(SnoopingProtectionNotificationBlockerInternalTest, AppNotifierTitles) {
  // App without known title uses a generic "app" string.
  const message_center::NotifierId unknown_app_notifier(
      message_center::NotifierType::APPLICATION, "unknown-app");
  const std::u16string unknown_app_title =
      hps_internal::GetNotifierTitle<FakeAppRegistryCache>(unknown_app_notifier,
                                                           AccountId());
  EXPECT_EQ(unknown_app_title,
            l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_APP_TITLE_LOWER));

  // Create an app cache with one used and one unused app.
  auto* app_cache =
      FakeAppRegistryCache::Get().GetAppRegistryCache(AccountId());
  apps::App crostini_app_state(apps::AppType::kCrostini, "crostini-app");
  crostini_app_state.name = "Signal Messenger";
  auto crostini_app = std::make_unique<apps::AppUpdate>(
      &crostini_app_state, /*delta=*/nullptr, AccountId());
  app_cache->AddApp(std::move(crostini_app));
  apps::App arc_app_state(apps::AppType::kArc, "arc-app");
  arc_app_state.name = "Discord";
  auto arc_app = std::make_unique<apps::AppUpdate>(
      &arc_app_state, /*delta=*/nullptr, AccountId());
  app_cache->AddApp(std::move(arc_app));

  // App with a registered name uses that name.
  const message_center::NotifierId crostini_app_notifier(
      message_center::NotifierType::CROSTINI_APPLICATION, "crostini-app");
  const std::u16string crostini_app_title =
      hps_internal::GetNotifierTitle<FakeAppRegistryCache>(
          crostini_app_notifier, AccountId());
  EXPECT_EQ(crostini_app_title, u"Signal Messenger");
}

TEST(SnoopingProtectionNotificationBlockerInternalTest, PopupMessage) {
  // Proper app names should be presented as-is.
  const std::vector<std::u16string> list_1 = {u"App title"};
  const std::u16string list_1_msg =
      hps_internal::GetTitlesBlockedMessage(list_1);
  EXPECT_TRUE(base::Contains(list_1_msg, u"App title"));

  // Improper app names should use a reasonable default. In this case, the
  // default should be capitalized since it is the first word in the message.
  const std::vector<std::u16string> list_2 = {l10n_util::GetStringUTF16(
      IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_LOWER)};
  const std::u16string list_2_msg =
      hps_internal::GetTitlesBlockedMessage(list_2);
  EXPECT_TRUE(base::Contains(
      list_2_msg,
      l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_UPPER)));

  // Subsequent improper app names should not be capitalized.
  const std::vector<std::u16string> list_3 = {
      u"App title",
      l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_LOWER)};
  const std::u16string list_3_msg =
      hps_internal::GetTitlesBlockedMessage(list_3);
  EXPECT_FALSE(base::Contains(
      list_3_msg,
      l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_UPPER)));
}

}  // namespace

}  // namespace ash
