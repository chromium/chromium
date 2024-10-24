// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include <memory>
#include <set>

#include "ash/constants/web_app_id_constants.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/message_center/public/cpp/notification.h"

const char kExampleUrl[] = "https://www.example.com";
const char kMicrosoft365PWAStartUrl[] =
    "https://www.microsoft365.com/?from=Homescreen";

namespace extensions {
class OfdsConfigPrivateApiBrowserTest : public ExtensionApiTest {
 public:
  OfdsConfigPrivateApiBrowserTest() = default;
  OfdsConfigPrivateApiBrowserTest(const OfdsConfigPrivateApiBrowserTest&) =
      delete;
  OfdsConfigPrivateApiBrowserTest& operator=(
      const OfdsConfigPrivateApiBrowserTest&) = delete;
  ~OfdsConfigPrivateApiBrowserTest() override = default;

 protected:
  auto GetAllNotifications() {
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(profile())->GetDisplayed(
        get_displayed_future.GetCallback());
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(profile());
    for (const std::string& notification_id : GetAllNotifications()) {
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
    }
  }

  size_t GetDisplayedNotificationsCount() {
    return GetAllNotifications().size();
  }

  void WaitUntilDisplayNotificationCount(size_t display_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return GetDisplayedNotificationsCount() == display_count;
    }));
  }

  void InstallMicrosoft365() {
    auto m365_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(kMicrosoft365PWAStartUrl));
    const webapps::AppId m365_app_id =
        web_app::test::InstallWebApp(profile(), std::move(m365_app_info));
    EXPECT_EQ(ash::kMicrosoft365AppId, m365_app_id);
  }

  int CreateNewTabAndNavigate(const GURL& url, Browser* browser) {
    chrome::NewTab(browser);

    content::WebContents* web_contents =
        browser->GetTabStripModel()->GetActiveWebContents();
    CreateSessionServiceTabHelper(web_contents);
    int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

    return tab_id;
  }
};

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       ShowAutomatedMountErrorNotificationIsShown) {
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function.get(), /*args=*/"[]", profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto notifications = GetAllNotifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_THAT(*notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       ShowAutomatedMountErrorNotificationCalledTwice) {
  auto function_first_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function_first_call.get(), /*args=*/"[]",
                              profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto notifications = GetAllNotifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_THAT(*notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));

  auto function_second_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function_second_call.get(), /*args=*/"[]",
                              profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto second_notifications = GetAllNotifications();

  ASSERT_EQ(1u, second_notifications.size());
  EXPECT_THAT(*second_notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       OpenInOfficeAppIncognitoTab) {
  // Create a new incognito browser and initiate navigation
  auto* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  int tab_id = CreateNewTabAndNavigate(GURL(kExampleUrl), incognito_browser);
  EXPECT_EQ(GURL(kExampleUrl), incognito_browser->GetTabStripModel()
                                   ->GetActiveWebContents()
                                   ->GetVisibleURL());

  // Run extension API method openInOfficeApp in the incognito browser
  std::string args = base::StringPrintf(R"([%d])", tab_id);
  auto function_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateOpenInOfficeAppFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function_call.get(), args, profile());

  Browser* m365_browser = web_app::AppBrowserController::FindForWebApp(
      *(profile()), ash::kMicrosoft365AppId);
  EXPECT_FALSE(m365_browser);
  EXPECT_EQ(GURL(kExampleUrl), incognito_browser->GetTabStripModel()
                                   ->GetActiveWebContents()
                                   ->GetVisibleURL());
  EXPECT_EQ("Tabs from guest/incognito mode can't be opened in Office", error);
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       OpenInOfficeAppTabNotFound) {
  int tab_id = -1;
  CreateNewTabAndNavigate(GURL(kExampleUrl), browser());

  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());

  // Run extension API method openInOfficeApp with an invalid tab id
  std::string args = base::StringPrintf(R"([%d])", tab_id);
  auto function_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateOpenInOfficeAppFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function_call.get(), args, profile());

  Browser* m365_browser = web_app::AppBrowserController::FindForWebApp(
      *(profile()), ash::kMicrosoft365AppId);
  EXPECT_FALSE(m365_browser);
  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                           base::NumberToString(tab_id)),
            error);
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       OpenInOfficeAppNotInstalled) {
  int tab_id = CreateNewTabAndNavigate(GURL(kExampleUrl), browser());

  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());

  // Run extension API method openInOfficeApp with the tab id of the new tab.
  std::string args = base::StringPrintf(R"([%d])", tab_id);
  auto function_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateOpenInOfficeAppFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function_call.get(), args, profile());

  Browser* m365_browser = web_app::AppBrowserController::FindForWebApp(
      *(profile()), ash::kMicrosoft365AppId);
  EXPECT_FALSE(m365_browser);
  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
  EXPECT_EQ("Microsoft 365 PWA is not installed", error);
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       OpenInOfficeAppSuccessful) {
  InstallMicrosoft365();

  int tab_id = CreateNewTabAndNavigate(GURL(kExampleUrl), browser());

  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());

  // Run extension API method openInOfficeApp with the tab id of the new tab.
  std::string args = base::StringPrintf(R"([%d])", tab_id);
  auto function_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateOpenInOfficeAppFunction>();
  api_test_utils::RunFunction(function_call.get(), args, profile());

  // The tab was opened in a new M365 window
  Browser* m365_browser = web_app::AppBrowserController::FindForWebApp(
      *(profile()), ash::kMicrosoft365AppId);
  EXPECT_TRUE(m365_browser);
  EXPECT_EQ(GURL(kExampleUrl), m365_browser->GetTabStripModel()
                                   ->GetActiveWebContents()
                                   ->GetVisibleURL());

  // The tab no longer exists in the old browser.
  EXPECT_NE(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       OpenInOfficeAppAdditionalAppInstance) {
  InstallMicrosoft365();

  // Launch M365 PWA in a window.
  Browser* existing_m365_browser =
      web_app::LaunchWebAppBrowser(profile(), ash::kMicrosoft365AppId);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(
      existing_m365_browser, ash::kMicrosoft365AppId));
  EXPECT_EQ(GURL(kMicrosoft365PWAStartUrl),
            existing_m365_browser->GetTabStripModel()
                ->GetActiveWebContents()
                ->GetVisibleURL());

  // Navigate to the example URL in the normal Chrome Browser.
  int tab_id = CreateNewTabAndNavigate(GURL(kExampleUrl), browser());
  EXPECT_EQ(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());

  // Run extension API method openInOfficeApp with the tab id of the new tab.
  std::string args = base::StringPrintf(R"([%d])", tab_id);
  auto function_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateOpenInOfficeAppFunction>();
  api_test_utils::RunFunction(function_call.get(), args, profile());

  // The tab was opened in a new M365 window
  Browser* new_m365_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(
      new_m365_browser, ash::kMicrosoft365AppId));
  EXPECT_EQ(GURL(kExampleUrl), new_m365_browser->GetTabStripModel()
                                   ->GetActiveWebContents()
                                   ->GetVisibleURL());

  // The old M365 window was not changed, so there are now 3 browsers.
  EXPECT_EQ(3U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(
      existing_m365_browser, ash::kMicrosoft365AppId));
  EXPECT_EQ(GURL(kMicrosoft365PWAStartUrl),
            existing_m365_browser->GetTabStripModel()
                ->GetActiveWebContents()
                ->GetVisibleURL());

  // The tab no longer exists in the old browser.
  EXPECT_NE(
      GURL(kExampleUrl),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

}  // namespace extensions
