// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/message_center/public/cpp/notification.h"

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
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(server_root_);
    ASSERT_TRUE(https_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());

    SiteEngagementScore::SetParamValuesForTesting();
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
        browser()->profile(), TestPageUrl().GetOrigin(), CONTENT_SETTING_ALLOW);
  }

  // Blocks permission to display Web Notifications for origin of the test
  // page that's being used in this browser test.
  void BlockNotificationPermissionForTest() const {
    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), TestPageUrl().GetOrigin(), CONTENT_SETTING_BLOCK);
  }

  bool RequestAndAcceptPermission() {
    return "granted" ==
           RequestAndRespondToPermission(PermissionRequestManager::ACCEPT_ALL);
  }

  double GetEngagementScore(const GURL& origin) const {
    return SiteEngagementService::Get(browser()->profile())->GetScore(origin);
  }

  GURL GetLastCommittedURL() const {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  // Navigates the browser to the test page indicated by |path|.
  void NavigateToTestPage(const std::string& path) const {
    ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(path));
  }

  // Executes |script| and stores the result as a string in |result|. A boolean
  // will be returned, indicating whether the script was executed successfully.
  bool RunScript(const std::string& script, std::string* result) const {
    return content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        script, result);
  }

  GURL TestPageUrl() const {
    return https_server_->GetURL(std::string("/") + kTestFileName);
  }

  std::string RequestAndRespondToPermission(
      PermissionRequestManager::AutoResponseType bubble_response) {
    std::string result;
    content::WebContents* web_contents = GetActiveWebContents(browser());
    PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(bubble_response);
    EXPECT_TRUE(RunScript("RequestPermission();", &result));
    return result;
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

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_none')",
       &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
      base::nullopt /* action_index */, base::nullopt /* reply */);

  // We expect +1 engagement for the notification interaction.
  EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

  // Clicking on the notification should not automatically close it.
  notifications = GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notifications[0].fullscreen_visibility());

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_none", script_result);

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

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayNonPersistentNotification('Title')", &script_result));
  EXPECT_EQ("ok", script_result);

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
  EXPECT_NEAR(default_notification.timestamp().ToJsTime(),
              base::Time::Now().ToJsTime(), 30 * 1000);

  // Now, test the non-default values.

  ASSERT_TRUE(RunScript(
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
        }))",
      &script_result));
  EXPECT_EQ("ok", script_result);

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
  EXPECT_DOUBLE_EQ(621046800000., notification.timestamp().ToJsTime());

#if !defined(OS_MACOSX)
  EXPECT_FALSE(notification.image().IsEmpty());
  EXPECT_EQ(kIconWidth, notification.image().Width());
  EXPECT_EQ(kIconHeight, notification.image().Height());
#endif

  EXPECT_FALSE(notification.icon().IsEmpty());
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
  EXPECT_FALSE(notification.small_image().IsEmpty());

  // Test that notifications with the same tag replace each other and have
  // identical ids.

  ASSERT_TRUE(RunScript(
      R"(DisplayNonPersistentNotification('Title3', {
          tag: 'replace-id'
        }))",
      &script_result));
  EXPECT_EQ("ok", script_result);

  notifications = GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(2u, notifications.size());

  message_center::Notification replacement = notifications[1];
  EXPECT_EQ("Title3", base::UTF16ToUTF8(replacement.title()));
  EXPECT_EQ(notification.id(), replacement.id());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayAndCloseNonPersistentNotification) {
  GrantNotificationPermissionForTest();

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayNonPersistentNotification('Title1')", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_TRUE(RunScript("DisplayAndCloseNonPersistentNotification('Title2')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  // Check that the first notification is still displayed and no others.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(base::ASCIIToUTF16("Title1"), notifications[0].title());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationOptionsReflection) {
  GrantNotificationPermissionForTest();

  // First, test the default values.

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('Some title', {})",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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

  // Verifies that the notification's default timestamp is set in the last 30
  // seconds. This number has no significance, but it needs to be significantly
  // high to avoid flakiness in the test.
  EXPECT_NEAR(default_notification.timestamp().ToJsTime(),
              base::Time::Now().ToJsTime(), 30 * 1000);

  // Now, test the non-default values.

  ASSERT_TRUE(RunScript("DisplayPersistentAllOptionsNotification()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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
#if !defined(OS_MACOSX)
  EXPECT_FALSE(all_options_notification.image().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.image().Width());
  EXPECT_EQ(kIconHeight, all_options_notification.image().Height());
#endif
  EXPECT_FALSE(all_options_notification.icon().IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.icon().Width());
  EXPECT_EQ(kIconHeight, all_options_notification.icon().Height());
  EXPECT_FALSE(all_options_notification.small_image().IsEmpty());
  EXPECT_TRUE(all_options_notification.renotify());
  EXPECT_TRUE(all_options_notification.silent());
  EXPECT_TRUE(all_options_notification.never_timeout());
  EXPECT_DOUBLE_EQ(kNotificationTimestamp,
                   all_options_notification.timestamp().ToJsTime());
  EXPECT_EQ(1u, all_options_notification.buttons().size());
  EXPECT_EQ("actionTitle",
            base::UTF16ToUTF8(all_options_notification.buttons()[0].title));
  EXPECT_FALSE(all_options_notification.buttons()[0].icon.IsEmpty());
  EXPECT_EQ(kIconWidth, all_options_notification.buttons()[0].icon.Width());
  EXPECT_EQ(kIconHeight, all_options_notification.buttons()[0].icon.Height());
}

// Chrome OS shows the notification settings inline.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       WebNotificationSiteSettingsButton) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL origin = web_contents->GetLastCommittedURL();
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(origin));

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('Some title', {})",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationVibrate()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_close')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  {
    base::RunLoop notification_closed_run_loop;
    display_service_tester_->SetNotificationClosedClosure(
        notification_closed_run_loop.QuitClosure());

    display_service_tester_->SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        base::nullopt /* action_index */, base::nullopt /* reply */);

    // We have interacted with the button, so expect a notification bump.
    EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

    ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
    EXPECT_EQ("action_close", script_result);

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

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayPersistentNotification('close_test')", &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
      true /* by_user */);

  // The user closed this notification so the score should remain the same.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("closing notification: close_test", script_result);

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
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));
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
    std::string script_result;
    ASSERT_TRUE(RunScript(
        R"(DisplayPersistentNotification('action_close', {
            tag: 'tag-1'
        }))",
        &script_result));
    EXPECT_EQ("ok", script_result);

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(true /* is_persistent */);
    ASSERT_EQ(1u, notifications.size());

    display_service_tester_->SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        base::nullopt /* action_index */, base::nullopt /* reply */);

    ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
    EXPECT_EQ("action_close", script_result);
  }
  {
    std::string script_result;
    ASSERT_TRUE(RunScript(
        R"(DisplayPersistentNotification('close_test', {
            tag: 'tag-1'
        }))",
        &script_result));
    EXPECT_EQ("ok", script_result);

    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications(true /* is_persistent */);
    ASSERT_EQ(1u, notifications.size());

    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::WEB_PERSISTENT, notifications[0].id(),
        true /* by_user */);

    ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
    EXPECT_EQ("closing notification: close_test", script_result);
  }
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestDisplayOriginContextMessage) {
  RequestAndAcceptPermission();

  // Creates a simple notification.
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

  GURL test_origin = TestPageUrl().GetOrigin();

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
  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification()", &script_result));

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
                       CheckFilePermissionNotGranted) {
  // This case should succeed because a normal page URL is used.
  std::string script_result;

  PermissionManager* permission_manager =
      PermissionManager::Get(browser()->profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                      TestPageUrl(), TestPageUrl())
                .content_setting);

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_manager
                ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                      TestPageUrl(), TestPageUrl())
                .content_setting);

  // This case should fail because a file URL is used.
  base::FilePath dir_source_root;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  base::FilePath full_file_path =
      dir_source_root.Append(server_root_).AppendASCII(kTestFileName);
  GURL file_url(net::FilePathToFileURL(full_file_path));

  ui_test_utils::NavigateToURL(browser(), file_url);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                      file_url, file_url)
                .content_setting);

  RequestAndAcceptPermission();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                      file_url, file_url)
                .content_setting)
      << "If this test fails, you may have fixed a bug preventing file origins "
      << "from sending their origin from Blink; if so you need to update the "
      << "display function for notification origins to show the file path.";
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DataUrlAsNotificationImage) {
  GrantNotificationPermissionForTest();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationDataUrlImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Data URL Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       BlobAsNotificationImage) {
  GrantNotificationPermissionForTest();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationBlobImage()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  EXPECT_FALSE(notification.icon().IsEmpty());

  EXPECT_EQ("Blob Title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(kIconWidth, notification.icon().Width());
  EXPECT_EQ(kIconHeight, notification.icon().Height());
}

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       DisplayPersistentNotificationWithActionButtons) {
  GrantNotificationPermissionForTest();

  // Expect 0.5 engagement for the navigation.
  EXPECT_DOUBLE_EQ(0.5, GetEngagementScore(GetLastCommittedURL()));

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationWithActionButtons()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(2u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));
  EXPECT_EQ("actionTitle2", base::UTF16ToUTF8(notification.buttons()[1].title));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      0 /* action_index */, base::nullopt /* reply */);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId1", script_result);
  EXPECT_DOUBLE_EQ(1.5, GetEngagementScore(GetLastCommittedURL()));

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedActionButton"));
  histogram_tester_.ExpectUniqueSample(
      "Notifications.PersistentWebNotificationClickResult",
      0 /* SERVICE_WORKER_OK */, 1);

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      1 /* action_index */, base::nullopt /* reply */);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId2", script_result);
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

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotificationWithReplyButton()",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  const message_center::Notification& notification = notifications[0];
  ASSERT_EQ(1u, notification.buttons().size());
  EXPECT_EQ("actionTitle1", base::UTF16ToUTF8(notification.buttons()[0].title));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
      0 /* action_index */, base::ASCIIToUTF16("hello"));

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_result));
  EXPECT_EQ("action_button_click actionId1 hello", script_result);
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

  std::string script_result;
  std::string script_message;

  ASSERT_TRUE(RunScript("DisplayNonPersistentNotification('NonPersistent')",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('PersistentI')",
                        &script_result));
  EXPECT_EQ("ok", script_result);
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('PersistentII')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

  // Only the persistent ones should show.
  ASSERT_TRUE(RunScript("GetDisplayedNotifications()", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_message));

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

  ASSERT_TRUE(RunScript("GetDisplayedNotifications()", &script_result));
  EXPECT_EQ("ok", script_result);

  ASSERT_TRUE(RunScript("GetMessageFromWorker()", &script_message));
  notification_ids = base::SplitString(
      script_message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // The list of displayed notification Ids should have been updated.
  ASSERT_EQ(1u, notification_ids.size());
  ASSERT_EQ(notification_ids[0], first_id);
}

// TODO(crbug.com/1002602): Test is flaky on TSAN.
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
                     kTestNotificationId, base::nullopt /* action_index */,
                     base::nullopt /* reply */, run_loop.QuitClosure());
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
#if !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       TestShouldDisplayFullscreen) {
  GrantNotificationPermissionForTest();

  // Set the page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();

  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive())
      << "Browser is active after going fullscreen";

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('display_normal')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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
  ui_test_utils::NavigateToURL(other_browser, GURL("about:blank"));

  std::string script_result;
  ASSERT_TRUE(RunScript(
      "DisplayPersistentNotification('display_normal')", &script_result));
  EXPECT_EQ("ok", script_result);

  // Set the notifcation page fullscreen
  browser()->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();
  {
    FullscreenStateWaiter fs_state(browser(), true);
    fs_state.Wait();
  }

  // Set the other browser fullscreen
  other_browser->exclusive_access_manager()->fullscreen_controller()->
      ToggleBrowserFullscreenMode();
  {
    FullscreenStateWaiter fs_state(other_browser, true);
    fs_state.Wait();
  }

  ASSERT_TRUE(browser()->exclusive_access_manager()->context()->IsFullscreen());
  ASSERT_TRUE(
      other_browser->exclusive_access_manager()->context()->IsFullscreen());

  ASSERT_FALSE(browser()->window()->IsActive());
  ASSERT_TRUE(other_browser->window()->IsActive());

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(true /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notifications[0].fullscreen_visibility());
}

#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
IN_PROC_BROWSER_TEST_F(PlatformNotificationServiceBrowserTest,
                       KeepAliveRegistryPendingNotificationEvent) {
  RequestAndAcceptPermission();

  std::string script_result;
  ASSERT_TRUE(RunScript("DisplayPersistentNotification('action_none')",
                        &script_result));
  EXPECT_EQ("ok", script_result);

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
                   notifications[0].id(), base::nullopt /* action_index */,
                   base::nullopt /* reply */, run_loop.QuitClosure());

  // The asynchronous part of the click event will still be in progress, but
  // the keep alive registration should have been created.
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));

  // Finish the click event.
  run_loop.Run();

  ASSERT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT));
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

  std::string script_result;
  ASSERT_TRUE(
      RunScript("DisplayPersistentAllOptionsNotification()", &script_result));
  EXPECT_EQ("ok", script_result);

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

  std::string script_result;
  ASSERT_TRUE(RunScript(
      R"(DisplayNonPersistentNotification('Title2', {
          image: 'icon.png'
        }))",
      &script_result));
  EXPECT_EQ("ok", script_result);

  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications(false /* is_persistent */);
  ASSERT_EQ(1u, notifications.size());

  // Since the kNotificationContentImage kill switch has disabled images, the
  // notification should be shown without an image.
  EXPECT_TRUE(notifications[0].image().IsEmpty());
}
