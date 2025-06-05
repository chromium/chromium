// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_platform_bridge_mac.h"

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "chrome/browser/notifications/mac/stub_notification_dispatcher_mac.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

using message_center::Notification;

namespace {

class MockNotificationDispatcherMac : public StubNotificationDispatcherMac {
 public:
  MOCK_METHOD(void, CloseAllNotifications, (), (override));
};

}  // namespace

class NotificationPlatformBridgeMacTest : public testing::Test {
 public:
  NotificationPlatformBridgeMacTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Default");
  }

 protected:
  std::unique_ptr<Notification> CreateBanner(
      const char* title,
      const char* subtitle,
      const char* origin,
      const char* button1,
      const char* button2,
      const webapps::AppId& web_app_id = "") {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              /*require_interaction=*/false,
                              /*show_settings_button=*/true, web_app_id);
  }

  std::unique_ptr<Notification> CreateAlert(
      const char* title,
      const char* subtitle,
      const char* origin,
      const char* button1,
      const char* button2,
      const webapps::AppId& web_app_id = "") {
    return CreateNotification(title, subtitle, origin, button1, button2,
                              /*require_interaction=*/true,
                              /*show_settings_button=*/true, web_app_id);
  }

  std::unique_ptr<Notification> CreateNotification(
      const char* title,
      const char* subtitle,
      const char* origin,
      const char* button1,
      const char* button2,
      bool require_interaction,
      bool show_settings_button,
      const webapps::AppId& web_app_id = "") {
    message_center::RichNotificationData optional_fields;
    if (button1) {
      optional_fields.buttons.push_back(
          message_center::ButtonInfo(base::UTF8ToUTF16(button1)));
      if (button2) {
        optional_fields.buttons.push_back(
            message_center::ButtonInfo(base::UTF8ToUTF16(button2)));
      }
    }
    if (show_settings_button) {
      optional_fields.settings_button_handler =
          message_center::SettingsButtonHandler::DELEGATE;
    }

    GURL url = GURL(origin);

    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, "id1",
        base::UTF8ToUTF16(title), base::UTF8ToUTF16(subtitle), ui::ImageModel(),
        u"Notifier's Name", url,
        message_center::NotifierId(
            url, /*title=*/std::nullopt,
            web_app_id.empty() ? std::nullopt : std::make_optional(web_app_id)),
        optional_fields, new message_center::NotificationDelegate());
    if (require_interaction)
      notification->set_never_timeout(true);

    return notification;
  }

  std::unique_ptr<NotificationDispatcherMac> CreateBannerDispatcher() {
    auto banner_dispatcher = std::make_unique<StubNotificationDispatcherMac>();
    banner_dispatcher_ = banner_dispatcher->AsWeakPtr();
    return banner_dispatcher;
  }

  std::unique_ptr<NotificationDispatcherMac> CreateAlertDispatcher() {
    auto alert_dispatcher = std::make_unique<StubNotificationDispatcherMac>();
    alert_dispatcher_ = alert_dispatcher->AsWeakPtr();
    return alert_dispatcher;
  }

  NotificationPlatformBridgeMac::WebAppDispatcherFactory
  CreateWebAppDispatcherFactory() {
    return base::BindLambdaForTesting(
        [&](const webapps::AppId& web_app_id)
            -> std::unique_ptr<NotificationDispatcherMac> {
          auto dispatcher = std::make_unique<StubNotificationDispatcherMac>();
          web_app_dispatchers_[web_app_id] = dispatcher->AsWeakPtr();
          return dispatcher;
        });
  }

  StubNotificationDispatcherMac* banner_dispatcher() {
    return banner_dispatcher_.get();
  }

  StubNotificationDispatcherMac* alert_dispatcher() {
    return alert_dispatcher_.get();
  }

  StubNotificationDispatcherMac* dispatcher_for_web_app(
      const webapps::AppId& web_app_id) {
    auto it = web_app_dispatchers_.find(web_app_id);
    if (it == web_app_dispatchers_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  TestingProfile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  base::WeakPtr<StubNotificationDispatcherMac> banner_dispatcher_;
  base::WeakPtr<StubNotificationDispatcherMac> alert_dispatcher_;
  std::map<webapps::AppId, base::WeakPtr<StubNotificationDispatcherMac>>
      web_app_dispatchers_;
};

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoButtons) {
  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  const auto& notifications = banner_dispatcher()->notifications();

  EXPECT_EQ(1u, notifications.size());

  const auto& delivered_notification = notifications[0];
  EXPECT_EQ(u"Title", delivered_notification->title);
  EXPECT_EQ(u"Context", delivered_notification->body);
  EXPECT_EQ(u"gmail.com", delivered_notification->subtitle);
  EXPECT_TRUE(delivered_notification->buttons.empty());
  EXPECT_TRUE(delivered_notification->show_settings_button);
}

TEST_F(NotificationPlatformBridgeMacTest, TestIncognitoProfile) {
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);

  TestingProfile::Builder profile_builder;
  profile_builder.SetPath(profile()->GetPath());
  profile_builder.SetProfileName(profile()->GetProfileUserName());
  Profile* incogito_profile = profile_builder.BuildIncognito(profile());

  // Show two notifications with the same id from different profiles.
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, /*metadata=*/nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, incogito_profile,
                  *notification, /*metadata=*/nullptr);
  EXPECT_EQ(2u, banner_dispatcher()->notifications().size());

  // Close the one for the incognito profile.
  bridge->Close(incogito_profile, "id1");
  const auto& notifications = banner_dispatcher()->notifications();
  ASSERT_EQ(1u, notifications.size());

  // Expect that the remaining notification is for the regular profile.
  const auto& remaining_notification = notifications[0];
  EXPECT_EQ(false, remaining_notification->meta->id->profile->incognito);

  // Close the one for the regular profile.
  bridge->Close(profile(), "id1");
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayNoSettings) {
  std::unique_ptr<Notification> notification = CreateNotification(
      "Title", "Context", "https://gmail.com", nullptr, nullptr,
      /*require_interaction=*/false, /*show_settings_button=*/false);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  const auto& notifications = banner_dispatcher()->notifications();

  EXPECT_EQ(1u, notifications.size());

  const auto& delivered_notification = notifications[0];
  EXPECT_EQ(u"Title", delivered_notification->title);
  EXPECT_EQ(u"Context", delivered_notification->body);
  EXPECT_EQ(u"gmail.com", delivered_notification->subtitle);
  EXPECT_TRUE(delivered_notification->buttons.empty());
  EXPECT_FALSE(delivered_notification->show_settings_button);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayOneButton) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);

  const auto& notifications = banner_dispatcher()->notifications();
  EXPECT_EQ(1u, notifications.size());
  const auto& delivered_notification = notifications[0];
  EXPECT_EQ(u"Title", delivered_notification->title);
  EXPECT_EQ(u"Context", delivered_notification->body);
  EXPECT_EQ(u"gmail.com", delivered_notification->subtitle);
  EXPECT_EQ(1u, delivered_notification->buttons.size());
  EXPECT_TRUE(delivered_notification->show_settings_button);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayProgress) {
  std::unique_ptr<Notification> notification =
      CreateBanner("Title", "Context", "https://gmail.com", nullptr, nullptr);
  const int kSamplePercent = 10;

  notification->set_progress(kSamplePercent);
  notification->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);

  // Progress notifications are considered alerts
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
  const auto& displayed_alerts = alert_dispatcher()->notifications();
  ASSERT_EQ(1u, displayed_alerts.size());

  const auto& delivered_notification = displayed_alerts[0];
  std::u16string expected = base::FormatPercent(kSamplePercent) + u" - Title";
  EXPECT_EQ(expected, delivered_notification->title);
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestDisplayUpdatesExistingNotification) {
  std::unique_ptr<Notification> notification = CreateNotification(
      "Title", "Context", "https://gmail.com", nullptr, nullptr,
      /*require_interaction=*/false, /*show_settings_button=*/false);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);

  {
    const auto& notifications = banner_dispatcher()->notifications();
    ASSERT_EQ(1u, notifications.size());
    EXPECT_TRUE(alert_dispatcher()->notifications().empty());
    const auto& delivered_notification = notifications[0];
    EXPECT_EQ(u"Title", delivered_notification->title);
  }

  notification = CreateNotification(
      "New Title", "Context", "https://gmail.com", nullptr, nullptr,
      /*require_interaction=*/false, /*show_settings_button=*/false);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  {
    const auto& notifications = banner_dispatcher()->notifications();
    ASSERT_EQ(1u, notifications.size());
    EXPECT_TRUE(alert_dispatcher()->notifications().empty());
    const auto& delivered_notification = notifications[0];
    EXPECT_EQ(u"New Title", delivered_notification->title);
  }

  notification = CreateNotification(
      "New Title", "Context", "https://gmail.com", nullptr, nullptr,
      /*require_interaction=*/true, /*show_settings_button=*/false);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  {
    const auto& notifications = alert_dispatcher()->notifications();
    ASSERT_EQ(1u, notifications.size());
    EXPECT_TRUE(banner_dispatcher()->notifications().empty());
    const auto& delivered_notification = notifications[0];
    EXPECT_EQ(u"New Title", delivered_notification->title);
  }
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseNotification) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  EXPECT_EQ(1u, banner_dispatcher()->notifications().size());

  bridge->Close(profile(), "id1");
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestGetDisplayed) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, nullptr);
  EXPECT_EQ(1u, banner_dispatcher()->notifications().size());

  {
    base::test::TestFuture<std::set<std::string>, bool> notifications;
    bridge->GetDisplayed(profile(), notifications.GetCallback());
    EXPECT_EQ(1u, notifications.Get<0>().size());
  }
  {
    base::test::TestFuture<std::set<std::string>, bool> notifications;
    bridge->GetDisplayedForOrigin(profile(), GURL("https://gmail.com"),
                                  notifications.GetCallback());
    EXPECT_EQ(1u, notifications.Get<0>().size());
  }
  {
    base::test::TestFuture<std::set<std::string>, bool> notifications;
    bridge->GetDisplayedForOrigin(profile(), GURL("https://example.com"),
                                  notifications.GetCallback());
    EXPECT_TRUE(notifications.Get<0>().empty());
  }
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesNotifications) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  {
    auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
        CreateBannerDispatcher(), CreateAlertDispatcher(),
        CreateWebAppDispatcherFactory());
    EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                    *notification, nullptr);
    EXPECT_EQ(1u, banner_dispatcher()->notifications().size());
  }
}

TEST_F(NotificationPlatformBridgeMacTest,
       TestProfileShutdownRemovesNotifications) {
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());

  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", "Button 2");

  TestingProfile::Builder profile_builder;
  profile_builder.SetPath(profile()->GetPath());
  profile_builder.SetProfileName(profile()->GetProfileUserName());
  Profile* incognito_profile = profile_builder.BuildIncognito(profile());

  // Show two notifications with the same id from different profiles.
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  *notification, /*metadata=*/nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, incognito_profile,
                  *notification, /*metadata=*/nullptr);
  EXPECT_EQ(2u, banner_dispatcher()->notifications().size());

  // Start shutdown of the incognito profile.
  bridge->DisplayServiceShutDown(incognito_profile);

  // Expect all notifications for that profile to be closed.
  const auto& notifications = banner_dispatcher()->notifications();
  ASSERT_EQ(1u, notifications.size());
  const auto& remaining_notification = notifications[0];
  EXPECT_EQ(false, remaining_notification->meta->id->profile->incognito);
}

// Regression test for crbug.com/1182795
TEST_F(NotificationPlatformBridgeMacTest, TestNullProfileShutdown) {
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  // Emulate shutdown of the null profile.
  bridge->DisplayServiceShutDown(/*profile=*/nullptr);
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(), *alert,
                  nullptr);
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
  EXPECT_EQ(1u, alert_dispatcher()->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayBannerAndAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> banner = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *alert), nullptr);
  EXPECT_EQ(1u, banner_dispatcher()->notifications().size());
  EXPECT_EQ(1u, alert_dispatcher()->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestCloseAlert) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  EXPECT_EQ(0u, alert_dispatcher()->notifications().size());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(), *alert,
                  nullptr);
  EXPECT_EQ(1u, alert_dispatcher()->notifications().size());

  bridge->Close(profile(), "id1");
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTest, TestQuitRemovesBannersAndAlerts) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", nullptr);
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr);

  auto banner_dispatcher = std::make_unique<MockNotificationDispatcherMac>();
  MockNotificationDispatcherMac* banner_dispatcher_ptr =
      banner_dispatcher.get();
  auto alert_dispatcher = std::make_unique<MockNotificationDispatcherMac>();
  MockNotificationDispatcherMac* alert_dispatcher_ptr = alert_dispatcher.get();
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      std::move(banner_dispatcher), std::move(alert_dispatcher),
      CreateWebAppDispatcherFactory());

  EXPECT_EQ(0u, banner_dispatcher_ptr->notifications().size());
  EXPECT_EQ(0u, alert_dispatcher_ptr->notifications().size());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *notification), nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *alert), nullptr);
  EXPECT_EQ(1u, banner_dispatcher_ptr->notifications().size());
  EXPECT_EQ(1u, alert_dispatcher_ptr->notifications().size());

  // Destructing the bridge should close all alerts and banners.
  EXPECT_CALL(*banner_dispatcher_ptr, CloseAllNotifications());
  EXPECT_CALL(*alert_dispatcher_ptr, CloseAllNotifications());
  bridge.reset();
}

TEST_F(NotificationPlatformBridgeMacTest, TestDisplayETLDPlusOne) {
  std::unique_ptr<Notification> notification = CreateBanner(
      "Title", "Context", "https://overthelimit.hello.world.test.co.uk",
      "Button 1", nullptr);

  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://mail.appspot.com",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://tests.peter.sh",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id3", *notification), nullptr);

  notification = CreateBanner(
      "Title", "Context",
      "https://somereallylongsubdomainthatactuallyisanaliasfortests.peter.sh/",
      "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id4", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "http://localhost:8080",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id5", *notification), nullptr);

  notification = CreateBanner("Title", "Context", "https://93.186.186.172",
                              "Button 1", nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id6", *notification), nullptr);

  const auto& notifications = banner_dispatcher()->notifications();
  EXPECT_EQ(6u, notifications.size());
  EXPECT_EQ(u"test.co.uk", notifications[0]->subtitle);
  EXPECT_EQ(u"mail.appspot.com", notifications[1]->subtitle);
  EXPECT_EQ(u"tests.peter.sh", notifications[2]->subtitle);
  EXPECT_EQ(u"peter.sh", notifications[3]->subtitle);
  EXPECT_EQ(u"localhost:8080", notifications[4]->subtitle);
  EXPECT_EQ(u"93.186.186.172", notifications[5]->subtitle);
}

class NotificationPlatformBridgeMacTestWithNotificationAttribution
    : public NotificationPlatformBridgeMacTest {
 public:
  void SetUp() override {
    NotificationPlatformBridgeMacTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    installed_app_id_ = web_app::test::InstallDummyWebApp(
        profile(), "Web App Name", GURL("https://gmail.com"));
    // TODO(https://crbug.com/328437955): Remove this when OS integration
    // happens safely by default.
    AppShimRegistry::Get()->OnAppInstalledForProfile(installed_app_id_,
                                                     profile()->GetPath());
  }

 protected:
  webapps::AppId installed_app_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAppShimNotificationAttribution};
};

TEST_F(NotificationPlatformBridgeMacTestWithNotificationAttribution,
       BannersAndAlertsAreAttributed) {
  std::unique_ptr<Notification> alert =
      CreateAlert("Title", "Context", "https://gmail.com", "Button 1", nullptr,
                  installed_app_id_);
  std::unique_ptr<Notification> banner =
      CreateBanner("Title", "Context", "https://gmail.com", "Button 1", nullptr,
                   installed_app_id_);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id2", *alert), nullptr);
  EXPECT_EQ(0u, banner_dispatcher()->notifications().size());
  EXPECT_EQ(0u, alert_dispatcher()->notifications().size());
  auto* app_dispatcher = dispatcher_for_web_app(installed_app_id_);
  ASSERT_TRUE(app_dispatcher);
  EXPECT_EQ(2u, app_dispatcher->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTestWithNotificationAttribution,
       CloseNotificationInWebApp) {
  std::unique_ptr<Notification> banner =
      CreateBanner("Title", "Context", "https://gmail.com", "Button 1", nullptr,
                   installed_app_id_);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);
  auto* app_dispatcher = dispatcher_for_web_app(installed_app_id_);
  ASSERT_TRUE(app_dispatcher);
  EXPECT_EQ(1u, app_dispatcher->notifications().size());

  bridge->Close(profile(), "notification_id1");
  EXPECT_EQ(0u, app_dispatcher->notifications().size());
}

TEST_F(NotificationPlatformBridgeMacTestWithNotificationAttribution,
       DisplayMovesNotificationToWebApp) {
  std::unique_ptr<Notification> banner = CreateBanner(
      "Title", "Context", "https://gmail.com", "Button 1", /*button2=*/nullptr,
      /*web_app_id=*/"");
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);

  EXPECT_FALSE(dispatcher_for_web_app(installed_app_id_));
  EXPECT_TRUE(alert_dispatcher()->notifications().empty());
  EXPECT_EQ(1u, banner_dispatcher()->notifications().size());

  banner = CreateBanner("Title", "Context", "https://gmail.com", "Button 1",
                        /*button2=*/nullptr, installed_app_id_);
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);

  auto* app_dispatcher = dispatcher_for_web_app(installed_app_id_);
  ASSERT_TRUE(app_dispatcher);
  EXPECT_EQ(1u, app_dispatcher->notifications().size());
  EXPECT_TRUE(alert_dispatcher()->notifications().empty());
  EXPECT_TRUE(banner_dispatcher()->notifications().empty());
}

TEST_F(NotificationPlatformBridgeMacTestWithNotificationAttribution,
       GetDisplayed) {
  std::unique_ptr<Notification> banner =
      CreateBanner("Title", "Context", "https://gmail.com", "Button 1", nullptr,
                   installed_app_id_);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile(),
                  Notification("notification_id1", *banner), nullptr);
  auto* app_dispatcher = dispatcher_for_web_app(installed_app_id_);
  ASSERT_TRUE(app_dispatcher);
  EXPECT_EQ(1u, app_dispatcher->notifications().size());

  {
    base::test::TestFuture<std::set<std::string>, bool> future;
    bridge->GetDisplayed(profile(), future.GetCallback());
    auto [notifications, supports_synchronization] = future.Get();
    EXPECT_TRUE(notifications.empty());
    EXPECT_FALSE(supports_synchronization);
  }
  {
    base::test::TestFuture<std::set<std::string>, bool> future;
    bridge->GetDisplayedForOrigin(profile(), GURL("https://gmail.com"),
                                  future.GetCallback());
    auto [notifications, supports_synchronization] = future.Get();
    EXPECT_EQ(1u, notifications.size());
    EXPECT_TRUE(supports_synchronization);
  }
  {
    base::test::TestFuture<std::set<std::string>, bool> future;
    bridge->GetDisplayedForOrigin(profile(), GURL("https://example.com"),
                                  future.GetCallback());

    auto [notifications, supports_synchronization] = future.Get();
    EXPECT_TRUE(notifications.empty());
    EXPECT_TRUE(supports_synchronization);
  }
}

TEST_F(NotificationPlatformBridgeMacTestWithNotificationAttribution,
       GetDisplayedWithoutExistingDispatcher) {
  std::unique_ptr<Notification> banner =
      CreateBanner("Title", "Context", "https://gmail.com", "Button 1", nullptr,
                   installed_app_id_);
  auto bridge = std::make_unique<NotificationPlatformBridgeMac>(
      CreateBannerDispatcher(), CreateAlertDispatcher(),
      CreateWebAppDispatcherFactory());
  EXPECT_FALSE(dispatcher_for_web_app(installed_app_id_));

  {
    base::test::TestFuture<std::set<std::string>, bool> future;
    bridge->GetDisplayedForOrigin(profile(), GURL("https://gmail.com"),
                                  future.GetCallback());
    auto [notifications, supports_synchronization] = future.Get();
    EXPECT_EQ(0u, notifications.size());
    EXPECT_TRUE(supports_synchronization);
  }

  EXPECT_TRUE(dispatcher_for_web_app(installed_app_id_));
}
