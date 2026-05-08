// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "net/base/apple/url_conversions.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_show_state_waiter.h"
#include "ui/views/widget/widget.h"

namespace {

// Instructs the NSApp's delegate to open |url|.
void SendOpenUrlToAppController(const GURL& url) {
  [NSApp.delegate application:NSApp openURLs:@[ net::NSURLWithGURL(url) ]];
}

// Note: These tests interact with SharedController which requires the browser's
// focus. In browser_tests other tests that are running in parallel cause
// flakiness to test test. See: https://crbug.com/40925562

// -------------------AppControllerInteractiveUITest-------------------

using AppControllerInteractiveUITest = InteractiveBrowserTest;

// Regression test for https://crbug.com/40192595
IN_PROC_BROWSER_TEST_F(AppControllerInteractiveUITest, DeleteEphemeralProfile) {
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  Profile* profile = browser()->profile();

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(profile, app_controller.lastProfileIfLoaded);

  // Mark the profile as ephemeral.
  profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  EXPECT_TRUE(entry->IsEphemeral());

  // Add sentinel data to observe profile destruction. Ephemeral profiles are
  // destroyed immediately upon browser close.
  ProfileDestructionWaiter waiter(profile);

  // Close browser and wait for the profile to be deleted.
  CloseBrowserSynchronously(browser());
  waiter.Wait();
  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // Create a new profile and activate it.
  Profile& profile2 = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      profile_manager->user_data_dir().AppendASCII("Profile 2"));
  Browser* browser2 = CreateBrowser(&profile2);
  // This should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:browser2->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  ASSERT_EQ(&profile2, app_controller.lastProfileIfLoaded);
}

// -------------------AppControllerMainMenuInteractiveUITest-------------------

class AppControllerMainMenuInteractiveUITest : public InProcessBrowserTest {
 protected:
  AppControllerMainMenuInteractiveUITest() = default;
};

// Test switching from Regular to OTR profiles updates the history menu.
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuInteractiveUITest,
                       SwitchToIncognitoRemovesHistoryItems) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* app_controller = AppController.sharedController;

  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendOpenUrlToAppController(simple);

  Profile* profile = browser()->profile();
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 1u);

  // Load profile's History Service backend so it will be assigned to the
  // HistoryMenuBridge, or else this test will fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Verify that history bridge service is available for regular profiles.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
  BrowserWindowInterface* regular_browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();

  // Open a URL in Incognito window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), simple, WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  // Check that there are exactly 2 browsers (regular and incognito).
  EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());

  BrowserWindowInterface* inc_browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  EXPECT_TRUE(inc_browser->GetProfile()->IsIncognitoProfile());

  // Verify that history bridge service is not available in Incognito.
  EXPECT_FALSE([app_controller historyMenuBridge]->service());

  regular_browser->GetWindow()->Show();
  // Verify that history bridge service is available again.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
}

// Tests opening a new window from dock menu while incognito browser is opened.
// Regression test for https://crbug.com/40241549
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuInteractiveUITest,
                       WhileIncognitoBrowserIsOpened_NewWindow) {
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 1u);

  // Create an incognito browser.
  Profile* original_profile = browser()->profile();
  Browser* incognito_browser = CreateIncognitoBrowser(original_profile);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 2u);

  // Close the original browser.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 1u);
  EXPECT_EQ(incognito_browser,
            GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser());

  // Simulate click on "New Window".
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [app_controller commandDispatch:item];

  // Check that a new non-incognito browser is opened.
  Browser* new_browser = browser_created_observer.Wait();
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 2u);
  EXPECT_TRUE(new_browser->profile()->IsRegularProfile());
  EXPECT_EQ(original_profile, new_browser->profile());
}

// Test that when the ProfilePicker is shown, a reopen event focuses the
// ProfilePicker. See crbug.com/429522811.
IN_PROC_BROWSER_TEST_F(AppControllerInteractiveUITest,
                       ProfilePickerReopenFocus) {
  // Activate the Profile Picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));

  RunTestSequence(
      // Wait for it to be shown and minimize it.
      InAnyContext(WaitForShow(ProfilePickerView::kViewId)),
      InSameContext(Steps(
          Do([]() {
            views::Widget* widget =
                ProfilePicker::GetViewForTesting()->GetWidget();
            widget->Minimize();
            // Wait for it to be fully minimized.
            views::test::WaitForWidgetShowState(
                widget, ui::mojom::WindowShowState::kMinimized);
          }),

          // Close the browser so Picker is the only thing (minimized).
          Do([this]() { browser()->GetWindow()->Close(); }),
          WaitForHide(kBrowserViewElementId), Do([]() {
            EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
          }),

          // Simulate Reopen.
          // This should call ProfilePicker::Show() which unminimizes and
          // activates it.
          Do([]() {
            [AppController.sharedController applicationShouldHandleReopen:NSApp
                                                        hasVisibleWindows:YES];
          }),

          // Verify it is visible and active.
          Do([]() {
            views::Widget* widget =
                ProfilePicker::GetViewForTesting()->GetWidget();
            if (!widget->IsActive()) {
              views::test::WaitForWidgetActive(widget, true);
            }
            EXPECT_TRUE(widget->IsVisible());
            EXPECT_TRUE(widget->IsActive());

            // No browser should be opened.
            EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
          }))));
}

// ---------------AppControllerIncognitoSwitchInteractiveUITest----------------

class AppControllerIncognitoSwitchInteractiveUITest
    : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

// Regression test for https://crbug.com/40057229
IN_PROC_BROWSER_TEST_F(AppControllerIncognitoSwitchInteractiveUITest,
                       ObserveProfileDestruction) {
  // Chrome is launched in incognito.
  Profile* otr_profile = browser()->profile();
  EXPECT_EQ(otr_profile,
            otr_profile->GetPrimaryOTRProfile(/*create_if_needed=*/false));
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 1u);
  AppController* app_controller = AppController.sharedController;

  // The last profile is the incognito profile.
  EXPECT_EQ([app_controller lastProfileIfLoaded], otr_profile);
  // Destroy the incognito profile.
  ProfileDestructionWaiter waiter(otr_profile);
  CloseBrowserSynchronously(browser());
  waiter.Wait();
  // Check that |-lastProfileIfLoaded| is not pointing to released memory.
  EXPECT_NE([app_controller lastProfileIfLoaded], otr_profile);
}

}  // namespace
