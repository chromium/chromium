// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "net/base/mac/url_conversions.h"

namespace {

// Instructs the NSApp's delegate to open |url|.
void SendOpenUrlToAppController(const GURL& url) {
  [NSApp.delegate application:NSApp openURLs:@[ net::NSURLWithGURL(url) ]];
}

// -------------------AppControllerMainMenuInteractiveUITest-------------------

class AppControllerMainMenuInteractiveUITest : public InProcessBrowserTest {
 protected:
  AppControllerMainMenuInteractiveUITest() = default;
};

// Note: These tests interacts with SharedController which requires the browser's
// focus. In browser_tests other tests that are running in parallel cause
// flakiness to test test. See: https://crbug.com/1469960

// Test switching from Regular to OTR profiles updates the history menu.
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuInteractiveUITest,
                       SwitchToIncognitoRemovesHistoryItems) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* app_controller = AppController.sharedController;

  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendOpenUrlToAppController(simple);

  Profile* profile = browser()->profile();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  // Load profile's History Service backend so it will be assigned to the
  // HistoryMenuBridge, or else this test will fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Verify that history bridge service is available for regular profiles.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
  Browser* regular_browser = chrome::GetLastActiveBrowser();

  // Open a URL in Incognito window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), simple, WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  // Check that there are exactly 2 browsers (regular and incognito).
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  Browser* inc_browser = chrome::GetLastActiveBrowser();
  EXPECT_TRUE(inc_browser->profile()->IsIncognitoProfile());

  // Verify that history bridge service is not available in Incognito.
  EXPECT_FALSE([app_controller historyMenuBridge]->service());

  regular_browser->window()->Show();
  // Verify that history bridge service is available again.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
}

// Tests opening a new window from dock menu while incognito browser is opened.
// Regression test for https://crbug.com/1371923
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuInteractiveUITest,
                       WhileIncognitoBrowserIsOpened_NewWindow) {
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  // Close the current browser.
  Profile* profile = browser()->profile();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(incognito_browser, chrome::GetLastActiveBrowser());

  // Simulate click on "New Window".
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [app_controller commandDispatch:item];

  // Check that a new non-incognito browser is opened.
  Browser* new_browser = browser_added_observer.Wait();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_TRUE(new_browser->profile()->IsRegularProfile());
  EXPECT_EQ(profile, new_browser->profile());
}

}  // namespace
