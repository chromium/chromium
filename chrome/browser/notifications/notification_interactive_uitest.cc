// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/./ui/tabs/tab_enums.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/notification_interactive_uitest_support.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace {

class ToggledNotificationBlocker : public message_center::NotificationBlocker {
 public:
  ToggledNotificationBlocker()
      : message_center::NotificationBlocker(
            message_center::MessageCenter::Get()),
        notifications_enabled_(true) {}
  ToggledNotificationBlocker(const ToggledNotificationBlocker&) = delete;
  ToggledNotificationBlocker& operator=(const ToggledNotificationBlocker&) =
      delete;
  ~ToggledNotificationBlocker() override = default;

  void SetNotificationsEnabled(bool enabled) {
    if (notifications_enabled_ != enabled) {
      notifications_enabled_ = enabled;
      NotifyBlockingStateChanged();
    }
  }

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return notifications_enabled_;
  }

 private:
  bool notifications_enabled_;
};

#if !BUILDFLAG(IS_ANDROID)
// Browser test class that creates a fake monitor MediaStream device and auto
// selects it when requesting one via navigator.mediaDevices.getDisplayMedia().
class NotificationsTestWithFakeMediaStream : public NotificationsTest {
 public:
  NotificationsTestWithFakeMediaStream() = default;
  ~NotificationsTestWithFakeMediaStream() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NotificationsTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    "display-media-type=monitor");
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
  }
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace {

const char kExpectedIconUrl[] = "/notifications/no_such_file.png";

}  // namespace

// Flaky on Windows, Mac, Linux: http://crbug.com/437414.
IN_PROC_BROWSER_TEST_F(NotificationsTest, DISABLED_TestUserGestureInfobar) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/notifications/notifications_request_function.html")));

  // Request permission by calling request() while eval'ing an inline script;
  // That's considered a user gesture to webkit, and should produce an infobar.
  EXPECT_EQ(true,
            content::EvalJs(GetActiveWebContents(browser()), "request();"));

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(1U, infobar_manager->infobars().size());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateSimpleNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  GURL EXPECTED_ICON_URL = embedded_test_server()->GetURL(kExpectedIconUrl);
  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(u"My Title", (*notifications.rbegin())->title());
  EXPECT_EQ(u"My Body", (*notifications.rbegin())->message());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, NotificationBlockerTest) {
  ToggledNotificationBlocker blocker;
  blocker.Init();
  TestMessageCenterObserver observer;

  ASSERT_TRUE(embedded_test_server()->Start());
  message_center::MessageCenter::Get()->AddObserver(&observer);

  // Creates a simple notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  EXPECT_EQ(0, GetNotificationPopupCount());
  blocker.SetNotificationsEnabled(false);

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  EXPECT_EQ(0, GetNotificationPopupCount());
  EXPECT_EQ("", observer.last_displayed_id());

  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(1, GetNotificationPopupCount());
  EXPECT_NE("", observer.last_displayed_id());

  result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(0, GetNotificationPopupCount());

  message_center::MessageCenter::Get()->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a notification and closes it.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  ASSERT_EQ(1, GetNotificationCount());

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCancelNotification) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a notification and cancels it in the origin page.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string note_id = CreateSimpleNotification(browser(), true);
  EXPECT_NE(note_id, "-1");

  ASSERT_EQ(1, GetNotificationCount());
  ASSERT_TRUE(CancelNotification(note_id.c_str(), browser()));
  ASSERT_EQ(0, GetNotificationCount());
}

// Requests notification privileges and verifies the prompt appears.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPermissionRequestUIAppears) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  EXPECT_TRUE(RequestPermissionAndWait(browser()));
  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowOnPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Tries to create a notification & clicks 'allow' on the prompt.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  // This notification should not be shown because we do not have permission.
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());

  ASSERT_TRUE(RequestAndAcceptPermission(browser()));

  CreateSimpleNotification(browser(), true);
  EXPECT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyOnPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that no notification is created when Deny is chosen from prompt.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  ASSERT_TRUE(RequestAndDenyPermission(browser()));
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetDisabledContentSettings(&settings);
  EXPECT_TRUE(CheckOriginInSetting(settings, GetTestPageURL()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestClosePermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that no notification is created when prompt is dismissed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  CreateSimpleNotification(browser(), false);
  ASSERT_EQ(0, GetNotificationCount());
  ContentSettingsForOneType settings;
  GetDisabledContentSettings(&settings);
  EXPECT_EQ(0U, settings.size());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestPermissionAPI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that Notification.permission returns the right thing.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  AllowOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  EXPECT_EQ("granted", QueryPermissionStatus(browser()));

  DenyOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  EXPECT_EQ("denied", QueryPermissionStatus(browser()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTestWithPermissionsEmbargo,
                       TestPermissionEmbargo) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  // Verify embargo behaviour - automatically blocked after 3 dismisses.
  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("default", QueryPermissionStatus(browser()));

  ASSERT_TRUE(RequestAndDismissPermission(browser()));
  EXPECT_EQ("denied", QueryPermissionStatus(browser()));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that all domains can be allowed to show notifications.
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyNotificationsFromAllSites) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that no domain can show notifications.
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyDomainAndAllowAll) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that denying a domain and allowing all shouldn't show
  // notifications from the denied domain.
  DenyOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestAllowDomainAndDenyAll) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that allowing a domain and denying all others should show
  // notifications from the allowed domain.
  AllowOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestDenyAndThenAllowDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify that denying and again allowing should show notifications.
  DenyOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), false);
  EXPECT_EQ("-1", result);

  ASSERT_EQ(0, GetNotificationCount());

  AllowOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, InlinePermissionRevokeUkm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Verify able to create, deny, and close the notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  base::RunLoop origin_queried_waiter;
  ukm_recorder.SetOnAddEntryCallback(ukm::builders::Permission::kEntryName,
                                     origin_queried_waiter.QuitClosure());
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->DisableNotification(
      (*notifications.rbegin())->id());

  origin_queried_waiter.Run();

  auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries.front().get();

  ukm_recorder.ExpectEntrySourceHasUrl(entry,
                                       embedded_test_server()->base_url());
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(entry, "Source"),
      static_cast<int64_t>(permissions::PermissionSourceUI::INLINE_SETTINGS));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            content_settings_uma_util::ContentSettingTypeToHistogramValue(
                ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(permissions::PermissionAction::REVOKED));
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCreateDenyCloseNotifications) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Verify able to create, deny, and close the notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());

  DenyOrigin(GetTestPageURL().DeprecatedGetOriginAsURL());
  ContentSettingsForOneType settings;
  GetDisabledContentSettings(&settings);
  ASSERT_TRUE(CheckOriginInSetting(
      settings, GetTestPageURL().DeprecatedGetOriginAsURL()));

  EXPECT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user
  ASSERT_EQ(0, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCloseTabWithPermissionRequestUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that user can close tab when bubble present.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  EXPECT_TRUE(RequestPermissionAndWait(browser()));
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestCrashRendererNotificationRemain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test crashing renderer does not close or crash notification.
  AllowAllOrigins();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  CreateSimpleNotification(browser(), true);
  ASSERT_EQ(1, GetNotificationCount());
  CrashTab(browser(), 0);
  ASSERT_EQ(1, GetNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationReplacement) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that we can replace a notification using the replaceId.
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateNotification(
      browser(), true, "abc.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());

  result = CreateNotification(
      browser(), false, "no_such_file.png", "Title2", "Body2", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(u"Title2", (*notifications.rbegin())->title());
  EXPECT_EQ(u"Body2", (*notifications.rbegin())->message());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest,
                       TestNotificationReplacementReappearance) {
  message_center::MessageCenter::Get()->SetHasMessageCenterView(false);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Test that we can replace a notification using the tag, and that it will
  // cause the notification to reappear as a popup again.
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "abc.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationPopupCount());

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->ClickOnNotification(
      (*notifications.rbegin())->id());

  ASSERT_EQ(1, GetNotificationPopupCount());

  result = CreateNotification(
      browser(), true, "abc.png", "Title2", "Body2", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationPopupCount());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationValidIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "icon.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  auto* notification = *notifications.rbegin();

  EXPECT_EQ(100, notification->icon().Size().width());
  EXPECT_EQ(100, notification->icon().Size().height());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationInvalidIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  ASSERT_EQ(0, GetNotificationPopupCount());

  // Not supplying an icon URL.
  std::string result = CreateNotification(
      browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  auto* notification = *notifications.rbegin();
  EXPECT_TRUE(notification->icon().IsEmpty());

  // Supplying an invalid icon URL.
  result = CreateNotification(
      browser(), true, "invalid.png", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  notifications = message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());

  notification = *notifications.rbegin();
  EXPECT_TRUE(notification->icon().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestNotificationDoubleClose) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetTestPageURLForFile("notification-double-close.html")));
  ASSERT_EQ(0, GetNotificationPopupCount());

  std::string result = CreateNotification(
      browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  message_center::MessageCenter::Get()->RemoveNotification(
      (*notifications.rbegin())->id(),
      true);  // by_user

  ASSERT_EQ(0, GetNotificationCount());

  // Calling WebContents::IsCrashed() will return FALSE here, even if the WC did
  // crash. Work around this timing issue by creating another notification,
  // which requires interaction with the renderer process.
  result = CreateNotification(browser(), true, "", "Title1", "Body1", "chat");
  EXPECT_NE("-1", result);
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayNormal) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();

  // Because the webpage is not fullscreen, there will be no special fullscreen
  // visibility tagged on the notification.
  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            (*notifications.rbegin())->fullscreen_visibility());
}

IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayFullscreen) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  ASSERT_TRUE(embedded_test_server()->Start());

  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  // Set the page fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  ASSERT_TRUE(browser()->window()->IsActive());

  // Creates a simple notification.
  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);
  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();

  // Because the webpage is fullscreen, the fullscreen bit will be set.
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            (*notifications.rbegin())->fullscreen_visibility());
}

// The Fake OSX fullscreen window doesn't like drawing a second fullscreen
// window when another is visible.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayMultiFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AllowAllOrigins();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  Browser* other_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(other_browser, GURL("about:blank")));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  // Set the notification page fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Set the other browser fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(other_browser);

  ASSERT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_TRUE(other_browser->window()->IsFullscreen());

  ui_test_utils::BrowserActivationWaiter waiter(other_browser);
  waiter.WaitForActivation();
  ASSERT_FALSE(browser()->window()->IsActive());
  ASSERT_TRUE(other_browser->window()->IsActive());

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  // Because the second page is the top-most fullscreen, the fullscreen bit
  // won't be set.
  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            (*notifications.rbegin())->fullscreen_visibility());
}
#endif

// Verify that a notification is actually displayed when the webpage that
// creates it is fullscreen with the fullscreen notification flag turned on.
IN_PROC_BROWSER_TEST_F(NotificationsTest, TestShouldDisplayPopupNotification) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  ASSERT_TRUE(embedded_test_server()->Start());

  // Creates a simple notification.
  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));

  // Set the page fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      browser()->window()->GetNativeWindow()));

  std::string result = CreateSimpleNotification(browser(), true);
  EXPECT_NE("-1", result);

  ASSERT_EQ(1, GetNotificationCount());
  message_center::NotificationList::PopupNotifications notifications =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40721738): Test fails on Windows and macOS on the bots as
// there is no real display to test with. Need to find a way to run these
// without a display and figure out why Lacros is timing out. Tests pass locally
// with a real display.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ShouldQueueDuringScreenPresent \
  DISABLED_ShouldQueueDuringScreenPresent
#else
#define MAYBE_ShouldQueueDuringScreenPresent ShouldQueueDuringScreenPresent
#endif
IN_PROC_BROWSER_TEST_F(NotificationsTestWithFakeMediaStream,
                       MAYBE_ShouldQueueDuringScreenPresent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Start second server so we can test screen recording on secure connections.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  AllowAllOrigins();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestPageURL()));
  const int notification_tab = browser()->tab_strip_model()->active_index();

  // We should see displayed notifications by default.
  std::string result = CreateSimpleNotification(browser(), /*wait=*/false);
  EXPECT_NE("-1", result);
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(u"My Title", (*notifications.begin())->title());
  EXPECT_EQ(u"My Body", (*notifications.begin())->message());

  // Open a new tab to a diffent origin from the one that shows notifications.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/notifications/notification_tester.html")));
  const int screen_capture_tab = browser()->tab_strip_model()->active_index();

  // Start a screen capture session.
  content::WebContents* web_contents = GetActiveWebContents(browser());
  ASSERT_EQ("success", content::EvalJs(web_contents, "startScreenCapture();"));

  // Showing a notification during the screen capture session should show the
  // "Notifications muted" notification.
  browser()->tab_strip_model()->ActivateTabAt(notification_tab);
  result = CreateSimpleNotification(browser(), /*wait=*/false);
  EXPECT_NE("-1", result);
  notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(2u, notifications.size());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/1),
            (*notifications.begin())->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            (*notifications.begin())->message());

  // Showing another notification during the screen captuure session should
  // update the "Notifications muted" notification title.
  result = CreateSimpleNotification(browser(), /*wait=*/false);
  EXPECT_NE("-1", result);
  notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(2u, notifications.size());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                             /*count=*/2),
            (*notifications.begin())->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
            (*notifications.begin())->message());

  // Stop the screen capture session.
  browser()->tab_strip_model()->ActivateTabAt(screen_capture_tab);
  ASSERT_EQ("success", content::EvalJs(web_contents, "stopScreenCapture();"));

  // Stopping the screen capture session should display the queued notifications
  // and close the "Notifications muted" notification.
  notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(3u, notifications.size());
  for (const message_center::Notification* notification : notifications) {
    EXPECT_EQ(u"My Title", notification->title());
    EXPECT_EQ(u"My Body", notification->message());
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)
