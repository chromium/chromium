// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#endif

namespace {

// Dimensions of the icon.png resource in the notification test data directory.
constexpr int kIconWidth = 100;
constexpr int kIconHeight = 100;

constexpr int kNotificationVibrationPattern[] = {100, 200, 300};
constexpr double kNotificationTimestamp = 621046800000.;

const char kTestFileName[] = "notifications/platform_notification_service.html";
const char kTestNotificationOrigin[] = "https://example.com/";
const char kTestNotificationId[] = "random#notification-id";

}  // namespace

class PlatformNotificationServiceBrowserTest : public InProcessBrowserTest {
 public:
  PlatformNotificationServiceBrowserTest();
  ~PlatformNotificationServiceBrowserTest() override = default;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(server_root_);
    ASSERT_TRUE(https_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());

    site_engagement::SiteEngagementScore::SetParamValuesForTesting();
    NavigateToTestPage(std::string("/") + kTestFileName);
  }

  void TearDownOnMainThread() override {
    display_service_tester_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceFactory::GetForProfile(
        browser()->profile());
  }

  // Returns a vector with the Notification objects that are being displayed
  // by the notification display service. Synchronous.
  std::vector<message_center::Notification> GetDisplayedNotifications(
      bool is_persistent) const {
    NotificationHandler::Type type =
        is_persistent ? NotificationHandler::Type::WEB_PERSISTENT
                      : NotificationHandler::Type::WEB_NON_PERSISTENT;

    return display_service_tester_->GetDisplayedNotificationsForType(type);
  }

  // Grants permission to display Web Notifications for origin of the test
  // page that's being used in this browser test.
  void GrantNotificationPermissionForTest() const {
    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), TestPageUrl().DeprecatedGetOriginAsURL(),
        CONTENT_SETTING_ALLOW);
  }

  // Blocks permission to display Web Notifications for origin of the test
  // page that's being used in this browser test.
  void BlockNotificationPermissionForTest() const {
    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), TestPageUrl().DeprecatedGetOriginAsURL(),
        CONTENT_SETTING_BLOCK);
  }

  bool RequestAndAcceptPermission() {
    return "granted" == RequestAndRespondToPermission(
                            permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  double GetEngagementScore(const GURL& origin) const {
    return site_engagement::SiteEngagementService::Get(browser()->profile())
        ->GetScore(origin);
  }

  GURL GetLastCommittedURL() const {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  // Navigates the browser to the test page indicated by |path|.
  void NavigateToTestPage(const std::string& path) const {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(path)));
  }

  // Executes |script| and stores the result as a string in |result|. A boolean
  // will be returned, indicating whether the script was executed successfully.
  content::EvalJsResult RunScript(const std::string& script) const {
    return content::EvalJs(browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetPrimaryMainFrame(),
                           script);
  }

  GURL TestPageUrl() const {
    return https_server_->GetURL(std::string("/") + kTestFileName);
  }

  content::EvalJsResult RequestAndRespondToPermission(
      permissions::PermissionRequestManager::AutoResponseType bubble_response) {
    std::string result;
    content::WebContents* web_contents = GetActiveWebContents(browser());
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(bubble_response);
    return RunScript("RequestPermission();");
  }

  content::WebContents* GetActiveWebContents(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  const base::FilePath server_root_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

PlatformNotificationServiceBrowserTest::PlatformNotificationServiceBrowserTest()
    : server_root_(FILE_PATH_LITERAL("chrome/test/data")) {}

// TODO(peter): Move PlatformNotificationService-related tests over from
// notification_interactive_uitest.cc to this file.

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithPermission) {
  RequestAndAcceptPermission();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('action_none')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
      std::nullopt /* action_index */, std::nullopt /* reply */);

  // We expect +1 engagement for the notification interaction.
  EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

  // Clicking on the notification should not automatically close it.
  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notifications[0].fullscreen_visibility());

  EXPECT_EQ("action_none", RunScript("GetMessageFromWorker()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // Check UMA was recorded.
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount("Notifications.Persistent.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.Clicked"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationClickResult",
      0 /* SERVICE_WORKER_OK */, 1);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       NonPersistentWebNotificationOptionsReflection) {
  GrantNotificationPermissionForTest();

  // First, test the default values.

  EXPECT_EQ("ok", RunScript("DisplayNonPersistentNotification('Title')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // We don't use the notification's direction or language, hence we don't check
  // those properties here.
  const message_center::Notification& default_notification = notifications[0];
  EXPECT_EQ("Title", base::UTF16ToUTF8(default_notification.title()));
  EXPECT_EQ("", base::UTF16ToUTF8(default_notification.message()));
  EXPECT_TRUE(default_notification.image().IsEmpty());
  EXPECT_TRUE(default_notification.icon().IsEmpty());
  EXPECT_TRUE(default_notification.small_image().IsEmpty());
  EXPECT_FALSE(default_notification.renotify());
  EXPECT_FALSE(default_notification.silent());
  EXPECT_FALSE(default_notification.never_timeout());
  EXPECT_EQ(0u, default_notification.buttons().size());

  // Verify that the notification's default timestamp is set in the last 30
  // seconds. (30 has no significance, just needs to be significantly high to
  // avoid test flakiness.)
  EXPECT_NEAR(default_notification.timestamp().InSecondsFSinceUnixEpoch(),
              base::Time::Now().InSecondsFSinceUnixEpoch(), 30);

  // Now, test the non-default values.

  EXPECT_EQ("ok", RunScript(
                      R"(DisplayNonPersistentNotification('Title2', {
          body: 'Contents',
          tag: 'replace-id',
          dir: 'rtl',
          lang: 'nl-NL',
          image: 'icon.png',
          icon: 'icon.png',
          badge: 'icon.png',
          timestamp: 621046800000,
          vibrate: [500, 200, 100],
          renotify: true,
          requireInteraction: true,
          data: [
            { property: 'value' }
          ]
        }))"));

  notifications = GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  message_center::Notification notification = notifications[1];
  EXPECT_EQ("Title2", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));

  // The js-provided tag should be part of the id.
  EXPECT_FALSE(notification.id().find("replace-id") == std::string::npos);

  EXPECT_THAT(notification.vibration_pattern(),
              testing::ElementsAreArray({500, 200, 100}));

  EXPECT_TRUE(notification.renotify());
  EXPECT_TRUE(notification.never_timeout());
  EXPECT_DOUBLE_EQ(621046800000.,
                   notification.timestamp().InMillisecondsFSinceUnixEpoch());

#if !BUILDFLAG(IS_MAC)
  EXPECT_FALSE(notification.image().IsEmpty());
  EXPECT_EQ(kIconWidth, notification.image().Width());
  EXPECT_EQ(kIconHeight, notification.image().Height());
#endif

  EXPECT_FALSE(notification.icon().IsEmpty());
  EXPECT_EQ(kIconWidth, notification.icon().Size().width());
  EXPECT_EQ(kIconHeight, notification.icon().Size().height());
  EXPECT_FALSE(notification.small_image().IsEmpty());

  // Test that notifications with the same tag replace each other and have
  // identical ids.

  EXPECT_EQ("ok", RunScript(
                      R"(DisplayNonPersistentNotification('Title3', {
          tag: 'replace-id'
        }))"));

  notifications = GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  message_center::Notification replacement = notifications[1];
  EXPECT_EQ("Title3", base::UTF16ToUTF8(replacement.title()));
  EXPECT_EQ(notification.id(), replacement.id());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayAndCloseNonPersistentNotification) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayNonPersistentNotification('Title1')"));

  EXPECT_EQ("ok",
            RunScript("DisplayAndCloseNonPersistentNotification('Title2')"));

  // Check that the first notification is still displayed and no others.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(u"Title1", notifications[0].title());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsReflection) {
  GrantNotificationPermissionForTest();

  // First, test the default values.

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('Some title', {})"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // We don't use the notification's direction or language, hence we don't check
  // those properties here.
  const message_center::Notification& default_notification = notifications[0];
  EXPECT_EQ("Some title", base::UTF16ToUTF8(default_notification.title()));
  EXPECT_EQ("", base::UTF16ToUTF8(default_notification.message()));
  EXPECT_TRUE(default_notification.image().IsEmpty());
  EXPECT_TRUE(default_notification.icon().IsEmpty());
  EXPECT_TRUE(default_notification.small_image().IsEmpty());
  EXPECT_FALSE(default_notification.renotify());
  EXPECT_FALSE(default_notification.silent());
  EXPECT_FALSE(default_notification.never_timeout());
  EXPECT_EQ(0u, default_notification.buttons().size());
  EXPECT_EQ(message_center::NotificationScenario::DEFAULT,
            default_notification.scenario());

  // Verifies that the notification's default timestamp is set in the last 30
  // seconds. This number has no significance, but it needs to be significantly
  // high to avoid flakiness in the test.
  EXPECT_NEAR(default_notification.timestamp().InSecondsFSinceUnixEpoch(),
              base::Time::Now().InSecondsFSinceUnixEpoch(), 30);

  // Now, test the non-default values.

  EXPECT_EQ("ok", RunScript("DisplayPersistentAllOptionsNotification()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  // We don't use the notification's direction or language, hence we don't check
  // those properties here.
  const message_center::Notification& all_options_notification =
      notifications[1];
  EXPECT_EQ("Title", base::UTF16ToUTF8(all_options_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(all_options_notification.message()));
  // The js-provided tag should be part of the id.
  EXPECT_FALSE(all_options_notification.id().find("replace-id") ==
               std::string::npos);
#if !BUILDFLAG(IS_MAC)
  EXPECT_FALSE(all_options_notification.image().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.image().Width());
  EXPECT_EQ(kIconHeight, all_options_notification.image().Height());
#endif
  EXPECT_FALSE(all_options_notification.icon().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.icon().Size().width());
  EXPECT_EQ(kIconHeight, all_options_notification.icon().Size().height());
  EXPECT_FALSE(all_options_notification.small_image().IsEmpty());
  EXPECT_TRUE(all_options_notification.renotify());
  EXPECT_TRUE(all_options_notification.silent());
  EXPECT_TRUE(all_options_notification.never_timeout());
  EXPECT_DOUBLE_EQ(
      kNotificationTimestamp,
      all_options_notification.timestamp().InMillisecondsFSinceUnixEpoch());
  EXPECT_EQ(1u, all_options_notification.buttons().size());
  EXPECT_EQ("actionTitle",
            base::UTF16ToUTF8(all_options_notification.buttons()[0].title));
  EXPECT_FALSE(all_options_notification.buttons()[0].icon.IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.buttons()[0].icon.Width());
  EXPECT_EQ(kIconHeight, all_options_notification.buttons()[0].icon.Height());
  EXPECT_EQ(message_center::ButtonType::DEFAULT,
            all_options_notification.buttons()[0].type);
}

// Chrome OS shows the notification settings inline.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationSiteSettingsButton) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL origin = web_contents->GetLastCommittedURL();
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(origin));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('Some title', {})"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(0u, notifications[0].buttons().size());

  display_service_tester_->SimulateSettingsClick(
      NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id());

  // Clicking on the settings button should not close the notification.
  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  // We see some timeouts in dbg tests, so increase the wait timeout to the
  // test launcher's timeout.
  const base::test::ScopedRunLoopTimeout specific_timeout(
          FROM_HERE, TestTimeouts::test_launcher_timeout());
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // No engagement should be granted for clicking on the settings link.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(origin));

  std::string url = web_contents->GetLastCommittedURL().spec();
  ASSERT_EQ(content::GetWebUIURLString("settings/content/notifications"), url);
}
#endif

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsVibrationPattern) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotificationVibrate()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CloseDisplayedPersistentNotification) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('action_close')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  {
    base::RunLoop notification_closed_run_loop;
    display_service_tester_->SetNotificationClosedClosure(
        notification_closed_run_loop.QuitClosure());

    display_service_tester_->SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        std::nullopt /* action_index */, std::nullopt /* reply */);

    // We have interacted with the button, so expect a notification bump.
    EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

    EXPECT_EQ("action_close", RunScript("GetMessageFromWorker()"));

    notification_closed_run_loop.Run();
  }

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(0u, notifications.size());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       UserClosesPersistentNotification) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('close_test')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
      true /* by_user */);

  // The user closed this notification so the score should remain the same.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("closing notification: close_test",
            RunScript("GetMessageFromWorker()"));

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClosedByUser"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationCloseResult",
      0 /* SERVICE_WORKER_OK */, 1);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       BlockingPermissionClosesNotification) {
  GrantNotificationPermissionForTest();

  // Creates a simple notification.
  ASSERT_EQ("ok", RunScript("DisplayPersistentNotification()"));
  ASSERT_EQ(1u, GetDisplayedNotifications(true /* is_persistent */).size());

  // Block permissions and wait until notification got closed.
  base::RunLoop run_loop;
  display_service_tester_->SetNotificationClosedClosure(run_loop.QuitClosure());
  BlockNotificationPermissionForTest();
  run_loop.Run();

  // Notification should be closed after blocking permissions.
  ASSERT_EQ(0u, GetDisplayedNotifications(true /* is_persistent */).size());

  // We are still in the process of closing the notification, but the recording
  // of the number of closed notifications happens after that. Run loop until
  // that is done to verify the UMA entry.
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Notifications.Permissions.RevokeDeleteCount",
      1 /* deleted_notifications */, 1 /* sample_count */);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       CloseAndUserClosePersistentNotificationWithTag) {
  GrantNotificationPermissionForTest();
  {
    EXPECT_EQ("ok", RunScript(
                        R"(DisplayPersistentNotification('action_close', {
            tag: 'tag-1'
        }))"));

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(true /* is_persistent */);
    ASSERT_EQ(1u, notifications.size());

    display_service_tester_->SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        std::nullopt /* action_index */, std::nullopt /* reply */);

    EXPECT_EQ("action_close", RunScript("GetMessageFromWorker()"));
  }
  {
    EXPECT_EQ("ok", RunScript(
                        R"(DisplayPersistentNotification('close_test', {
            tag: 'tag-1'
        }))"));

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(true /* is_persistent */);
    ASSERT_EQ(1u, notifications.size());

    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        true /* by_user */);

    EXPECT_EQ("closing notification: close_test",
              RunScript("GetMessageFromWorker()"));
  }
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestDisplayOriginContextMessage) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  ASSERT_EQ("ok", RunScript("DisplayPersistentNotification()"));

  GURL test_origin = TestPageUrl().DeprecatedGetOriginAsURL();

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_TRUE(notifications[0].context_message().empty());
  EXPECT_EQ(test_origin.spec(), notifications[0].origin_url().spec());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       PersistentNotificationServiceWorkerScope) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  ASSERT_EQ("ok", RunScript("DisplayPersistentNotification()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(
      TestPageUrl(),
      PersistentNotificationMetadata::From(
          display_service_tester_->GetMetadataForNotification(notifications[0]))
          ->service_worker_scope);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DataUrlAsNotificationImage) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotificationDataUrlImage()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Data URL Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Size().width());
  EXPECT_EQ(kIconHeight, notification.icon().Size().height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       BlobAsNotificationImage) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotificationBlobImage()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Blob Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Size().width());
  EXPECT_EQ(kIconHeight, notification.icon().Size().height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithActionButtons) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("ok",
            RunScript("DisplayPersistentNotificationWithActionButtons()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));
  EXPECT_EQ("actionTitle2", base::UTF16ToUTF8(notification.buttons()[1].title));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      0 /* action_index */, std::nullopt /* reply */);

  EXPECT_EQ("action_button_click actionId1",
            RunScript("GetMessageFromWorker()"));
  EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedActionButton"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationClickResult",
      0 /* SERVICE_WORKER_OK */, 1);

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      1 /* action_index */, std::nullopt /* reply */);

  EXPECT_EQ("action_button_click actionId2",
            RunScript("GetMessageFromWorker()"));
  EXPECT_DOUBLE_EQ(2.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ(2, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedActionButton"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationClickResult",
      0 /* SERVICE_WORKER_OK */, 2);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithReplyButton) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotificationWithReplyButton()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      0 /* action_index */, u"hello");

  EXPECT_EQ("action_button_click actionId1 hello",
            RunScript("GetMessageFromWorker()"));
  EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedActionButton"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationClickResult",
      0 /* SERVICE_WORKER_OK */, 1);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       GetDisplayedNotifications) {
  RequestAndAcceptPermission();

  EXPECT_EQ("ok",
            RunScript("DisplayNonPersistentNotification('NonPersistent')"));
  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('PersistentI')"));
  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('PersistentII')"));

  // Only the persistent ones should show.
  EXPECT_EQ("ok", RunScript("GetDisplayedNotifications()"));

  std::string script_message =
      RunScript("GetMessageFromWorker()").ExtractString();

  std::vector<std::string> notification_ids = base::SplitString(
      script_message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, notification_ids.size());

  const std::string first_id = notification_ids[0];

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  // Now remove one of the notifications straight from the ui manager
  // without going through the database.
  const message_center::Notification& notification = notifications[1];

  // p# is the prefix for persistent notifications. See
  //  content/browser/notifications/notification_id_generator.{h,cc} for details
  ASSERT_TRUE(
      base::StartsWith(notification.id(), "p#", base::CompareCase::SENSITIVE));

  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      false /* by_user */, true /* silent */);

  EXPECT_EQ("ok", RunScript("GetDisplayedNotifications()"));

  script_message = RunScript("GetMessageFromWorker()").ExtractString();
  notification_ids = base::SplitString(
      script_message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // The list of displayed notification Ids should have been updated.
  ASSERT_EQ(1u, notification_ids.size());
  ASSERT_EQ(notification_ids[0], first_id);
}

// TODO(crbug.com/40098231): Test is flaky on TSAN.
#if defined(THREAD_SANITIZER)
#define MAYBE_OrphanedNonPersistentNotificationCreatesForegroundTab \
  DISABLED_OrphanedNonPersistentNotificationCreatesForegroundTab
#else
#define MAYBE_OrphanedNonPersistentNotificationCreatesForegroundTab \
  OrphanedNonPersistentNotificationCreatesForegroundTab
#endif
IN_PROC_BROWSER_TEST_F(
    PlatformNotificationServiceBrowserTest,
    MAYBE_OrphanedNonPersistentNotificationCreatesForegroundTab) {
  // Verifies that activating a non-persistent notification that no longer has
  // any event listeners attached (e.g. because the tab closed) creates a new
  // foreground tab.

  Profile* profile = browser()->profile();

  NotificationDisplayServiceImpl* display_service =
      NotificationDisplayServiceImpl::GetForProfile(profile);
  ASSERT_TRUE(display_service);

  NotificationHandler* handler = display_service->GetNotificationHandler(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  ASSERT_TRUE(handler);

  // There should be one open tab for the current |browser()|.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  content::WebContents* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Signal an activation for a notification that's never been shown.
  {
    base::RunLoop run_loop;
    handler->OnClick(profile, GURL(kTestNotificationOrigin),
                     kTestNotificationId, std::nullopt /* action_index */,
                     std::nullopt /* reply */, run_loop.QuitClosure());
    run_loop.Run();
  }

  // A second tab should've been created and have been brought to the foreground
  // for the notification's test origin.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_NE(active_web_contents, original_web_contents);
  EXPECT_EQ(active_web_contents->GetVisibleURL(),
            GURL(kTestNotificationOrigin));
}

// Mac OS X exclusively uses native notifications, so the decision on whether to
// display notifications whilst fullscreen is deferred to the operating system.
#if !BUILDFLAG(IS_MAC)

// TODO(crbug.com/40132496) Test is flaky on Linux TSan.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    defined(THREAD_SANITIZER)
#define MAYBE_TestShouldDisplayFullscreen DISABLED_TestShouldDisplayFullscreen
#else
#define MAYBE_TestShouldDisplayFullscreen TestShouldDisplayFullscreen
#endif
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       MAYBE_TestShouldDisplayFullscreen) {
  GrantNotificationPermissionForTest();

  // Set the page fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ui_test_utils::BrowserActivationWaiter(browser()).WaitForActivation();
  ASSERT_TRUE(browser()->window()->IsActive())
      << "Browser is active after going fullscreen";

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('display_normal')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            notifications[0].fullscreen_visibility());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayMultiFullscreen) {
  ASSERT_NO_FATAL_FAILURE(GrantNotificationPermissionForTest());

  Browser* other_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(other_browser, GURL("about:blank")));

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('display_normal')"));

  // Set the notifcation page fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Set the other browser fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(other_browser);

  ASSERT_TRUE(browser()->exclusive_access_manager()->context()->IsFullscreen());
  ASSERT_TRUE(
      other_browser->exclusive_access_manager()->context()->IsFullscreen());

  ui_test_utils::BrowserActivationWaiter(other_browser).WaitForActivation();
  ASSERT_FALSE(browser()->window()->IsActive());
  ASSERT_TRUE(other_browser->window()->IsActive());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notifications[0].fullscreen_visibility());
}

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       KeepAliveRegistryPendingNotificationClickEvent) {
  RequestAndAcceptPermission();

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('action_none')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));

  NotificationDisplayServiceImpl* display_service =
      NotificationDisplayServiceImpl::GetForProfile(browser()->profile());
  NotificationHandler* handler = display_service->GetNotificationHandler(
      NotificationHandler::Type::WEB_PERSISTENT);
  ASSERT_TRUE(handler);

  base::RunLoop run_loop;
  handler->OnClick(browser()->profile(), notifications[0].origin_url(),
                   notifications[0].id(), std::nullopt /* action_index */,
                   std::nullopt /* reply */, run_loop.QuitClosure());

  // The asynchronous part of the click event will still be in progress, but
  // the keep alive registration should have been created.
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));

  // Finish the click event.
  run_loop.Run();

  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       KeepAliveRegistryPendingNotificationCloseEvent) {
  RequestAndAcceptPermission();

  EXPECT_EQ("ok", RunScript("DisplayPersistentNotification('action_none')"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT));

  NotificationDisplayServiceImpl* display_service =
      NotificationDisplayServiceImpl::GetForProfile(browser()->profile());
  NotificationHandler* handler = display_service->GetNotificationHandler(
      NotificationHandler::Type::WEB_PERSISTENT);
  ASSERT_TRUE(handler);

  base::RunLoop run_loop;
  handler->OnClose(browser()->profile(), notifications[0].origin_url(),
                   notifications[0].id(), true /* by_user */,
                   run_loop.QuitClosure());

  // The asynchronous part of the close event will still be in progress, but
  // the keep alive registration should have been created.
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT));

  // Finish the close event.
  run_loop.Run();

  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT));
}
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

class PlatformNotificationServiceWithoutContentImageBrowserTest
    : public PlatformNotificationServiceBrowserTest {
 public:
  // InProcessBrowserTest overrides.
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kNotificationContentImage});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PlatformNotificationServiceWithoutContentImageBrowserTest,
    KillSwitch) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayPersistentAllOptionsNotification()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // Since the kNotificationContentImage kill switch has disabled images, the
  // notification should be shown without an image.
  EXPECT_TRUE(notifications[0].image().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(
    PlatformNotificationServiceWithoutContentImageBrowserTest,
    KillSwitch_NonPersistentNotifications) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript(
                      R"(DisplayNonPersistentNotification('Title2', {
          image: 'icon.png'
        }))"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // Since the kNotificationContentImage kill switch has disabled images, the
  // notification should be shown without an image.
  EXPECT_TRUE(notifications[0].image().IsEmpty());
}

class PlatformNotificationServiceIncomingCallTest
    : public PlatformNotificationServiceBrowserTest {
 public:
  // InProcessBrowserTest overrides.
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kIncomingCallNotifications,
         features::kIncomingCallNotifications},
        {});
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceIncomingCallTest,
                       DisplayIncomingCallNotificationWithActionButtons) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok",
            RunScript("DisplayIncomingCallNotificationWithActionButton()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // When sent from an origin that does not have an installed web app, the
  // scenario should be set to DEFAULT and the default dismiss button should be
  // present.
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::DEFAULT,
            notification.scenario());
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));
  EXPECT_EQ(message_center::ButtonType::ACKNOWLEDGE,
            notification.buttons()[0].type);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            notification.buttons()[1].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            notification.buttons()[1].type);

  // Install the web app.
  const GURL web_app_url = TestPageUrl();
  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "Web App Title", web_app_url);

  EXPECT_EQ("ok",
            RunScript("DisplayIncomingCallNotificationWithActionButton()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  // After installing the origin's web app, the scenario is set to
  // INCOMING_CALL.
  const message_center::Notification& app_notification = notifications[1];
  EXPECT_EQ("Title", base::UTF16ToUTF8(app_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(app_notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::INCOMING_CALL,
            app_notification.scenario());
  ASSERT_EQ(2u, app_notification.buttons().size());
  EXPECT_EQ("actionTitle1",
            base::UTF16ToUTF8(app_notification.buttons()[0].title));
  EXPECT_EQ(message_center::ButtonType::ACKNOWLEDGE,
            app_notification.buttons()[0].type);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            app_notification.buttons()[1].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            app_notification.buttons()[1].type);

  web_app::test::UninstallWebApp(browser()->profile(), app_id);

  EXPECT_EQ("ok",
            RunScript("DisplayIncomingCallNotificationWithActionButton()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(3u, notifications.size());

  // After uninstalling the origin's web app, the scenario should be set
  // back to DEFAULT.
  const message_center::Notification& uninstall_notification = notifications[2];
  EXPECT_EQ("Title", base::UTF16ToUTF8(uninstall_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(uninstall_notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::DEFAULT,
            uninstall_notification.scenario());
  ASSERT_EQ(2u, uninstall_notification.buttons().size());
  EXPECT_EQ("actionTitle1",
            base::UTF16ToUTF8(uninstall_notification.buttons()[0].title));
  EXPECT_EQ(message_center::ButtonType::ACKNOWLEDGE,
            uninstall_notification.buttons()[0].type);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            uninstall_notification.buttons()[1].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            uninstall_notification.buttons()[1].type);
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceIncomingCallTest,
                       DisplayIncomingCallNotificationWithoutActionButtons) {
  GrantNotificationPermissionForTest();

  EXPECT_EQ("ok", RunScript("DisplayIncomingCallNotification()"));

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // When sent from an origin that does not have an installed web app, the
  // scenario should be set to DEFAULT and the default dismiss button should be
  // present, even though no action button was supplied by the origin.
  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ("Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::DEFAULT,
            notification.scenario());
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            notification.buttons()[0].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            notification.buttons()[0].type);

  // Install the web app.
  const GURL web_app_url = TestPageUrl();
  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "Web App Title", web_app_url);

  EXPECT_EQ("ok", RunScript("DisplayIncomingCallNotification()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  // After installing the origin's web app, the scenario is set to
  // INCOMING_CALL and the default dismiss button should be
  // present, even though no action button was supplied by the origin.
  const message_center::Notification& app_notification = notifications[1];
  EXPECT_EQ("Title", base::UTF16ToUTF8(app_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(app_notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::INCOMING_CALL,
            app_notification.scenario());
  ASSERT_EQ(1u, app_notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            app_notification.buttons()[0].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            app_notification.buttons()[0].type);

  web_app::test::UninstallWebApp(browser()->profile(), app_id);

  EXPECT_EQ("ok", RunScript("DisplayIncomingCallNotification()"));

  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(3u, notifications.size());

  // After uninstalling the origin's web app, the scenario should be set
  // back to DEFAULT and the default dismiss button should be
  // present, even though no action button was supplied by the origin.
  const message_center::Notification& uninstall_notification = notifications[2];
  EXPECT_EQ("Title", base::UTF16ToUTF8(uninstall_notification.title()));
  EXPECT_EQ("Contents", base::UTF16ToUTF8(uninstall_notification.message()));
  EXPECT_EQ(message_center::NotificationScenario::DEFAULT,
            uninstall_notification.scenario());
  ASSERT_EQ(1u, uninstall_notification.buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CLOSE),
            uninstall_notification.buttons()[0].title);
  EXPECT_EQ(message_center::ButtonType::DISMISS,
            uninstall_notification.buttons()[0].type);
}
