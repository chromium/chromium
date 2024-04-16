// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service.h"

#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using Window = sessions::tab_restore::Window;

class TabRestoreServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  TabRestoreServiceImplBrowserTest()
      : test_system_web_app_installation_(
            ash::TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp()) {}

 protected:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      test_system_web_app_installation_;
};

IN_PROC_BROWSER_TEST_F(TabRestoreServiceImplBrowserTest, RestoreApp) {
  Profile* profile = browser()->profile();
  sessions::TabRestoreService* trs =
      TabRestoreServiceFactory::GetForProfile(profile);
  const char* app_name = "TestApp";

  Browser* app_browser = CreateBrowserForApp(app_name, profile);
  CloseBrowserSynchronously(app_browser);

  // One entry should be created.
  ASSERT_EQ(1U, trs->entries().size());
  const sessions::tab_restore::Entry* restored_entry =
      trs->entries().front().get();

  // It should be a window with an app.
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, restored_entry->type);
  const Window* restored_window = static_cast<const Window*>(restored_entry);
  EXPECT_EQ(app_name, restored_window->app_name);
}

// Test that the last app browser tab saves the Window to ensure it will be
// reopened in the correct app browser.
IN_PROC_BROWSER_TEST_F(TabRestoreServiceImplBrowserTest,
                       LastAppTabSavesWindow) {
  Profile* profile = browser()->profile();
  sessions::TabRestoreService* trs =
      TabRestoreServiceFactory::GetForProfile(profile);

  test_system_web_app_installation_->WaitForAppInstall();
  Browser* app_browser = web_app::LaunchWebAppBrowser(
      browser()->profile(), test_system_web_app_installation_->GetAppId());
  GURL app_url = test_system_web_app_installation_->GetAppUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_url));

  // Create second tab and close it, TAB entry should be created.
  chrome::NewTab(app_browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_url));
  chrome::CloseTab(app_browser);
  ASSERT_EQ(1U, trs->entries().size());
  const sessions::tab_restore::Entry* tab_entry = trs->entries().front().get();
  EXPECT_EQ(sessions::tab_restore::Type::TAB, tab_entry->type);

  // Close last tab, WINDOW entry should be created.
  chrome::CloseTab(app_browser);
  EXPECT_EQ(2U, trs->entries().size());
  const sessions::tab_restore::Entry* window_entry =
      trs->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, window_entry->type);
  const Window* restored_window = static_cast<const Window*>(window_entry);
  EXPECT_EQ(app_browser->app_name(), restored_window->app_name);
  EXPECT_EQ(1U, restored_window->tabs.size());
}
