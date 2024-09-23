// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stddef.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/shortcuts/chrome_webloc_file.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

GURL g_open_shortcut_url;

// Instructs the NSApp's delegate to open |url|.
void SendOpenUrlToAppController(const GURL& url) {
  [NSApp.delegate application:NSApp openURLs:@[ net::NSURLWithGURL(url) ]];
}

Profile& CreateAndWaitForProfile(const base::FilePath& profile_dir) {
  Profile& profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(), profile_dir);
  return profile;
}

void CreateAndWaitForSystemProfile() {
  CreateAndWaitForProfile(ProfileManager::GetSystemProfilePath());
}

Profile& CreateAndWaitForGuestProfile() {
  return CreateAndWaitForProfile(ProfileManager::GetGuestProfilePath());
}

void SetGuestProfileAsLastProfile() {
  AppController* app_controller = AppController.sharedController;

  // Create the guest profile, and set it as the last used profile.
  Profile& guest_profile = CreateAndWaitForGuestProfile();
  [app_controller setLastProfile:&guest_profile];

  Profile* profile = [app_controller lastProfileIfLoaded];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile.GetPath(), profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());

  // Also set the last used profile path preference. If the profile does need to
  // be read from disk for some reason this acts as a backstop.
  g_browser_process->local_state()->SetString(
      prefs::kProfileLastUsed, guest_profile.GetPath().BaseName().value());
}

// Key for ProfileDestroyedData user data.
const char kProfileDestructionWaiterUserDataKey = 0;

// Waits until the Profile instance is destroyed.
class ProfileDestructionWaiter {
 public:
  explicit ProfileDestructionWaiter(Profile* profile) {
    profile->SetUserData(
        &kProfileDestructionWaiterUserDataKey,
        std::make_unique<ProfileDestroyedData>(run_loop_.QuitClosure()));
  }

  void Wait() { run_loop_.Run(); }

 private:
  // Simple user data that calls a callback at destruction.
  class ProfileDestroyedData : public base::SupportsUserData::Data {
   public:
    explicit ProfileDestroyedData(base::OnceClosure callback)
        : scoped_closure_runner_(std::move(callback)) {}

   private:
    base::ScopedClosureRunner scoped_closure_runner_;
  };

  base::RunLoop run_loop_;
};

}  // namespace

@interface TestOpenShortcutOnStartup : NSObject
- (void)applicationWillFinishLaunching:(NSNotification*)notification;
@end

@implementation TestOpenShortcutOnStartup

- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  if (!g_open_shortcut_url.is_valid())
    return;

  SendOpenUrlToAppController(g_open_shortcut_url);
}

@end

namespace {

using AppControllerBrowserTest = InProcessBrowserTest;

// Returns whether a window's pixels are actually on the screen, which is the
// case when it and all of its parents are marked visible.
bool IsReallyVisible(NSWindow* window) {
  while (window) {
    if (!window.visible)
      return false;
    window = [window parentWindow];
  }
  return true;
}

size_t CountVisibleWindows() {
  size_t count = 0;
  for (NSWindow* w in [NSApp windows])
    count = count + (IsReallyVisible(w) ? 1 : 0);
  return count;
}

// Returns how many visible NSWindows are expected for a given count of browser
// windows.
size_t ExpectedWindowCountForBrowserCount(size_t browsers) {
  return browsers;
}

// Test browser shutdown with a command in the message queue.
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest, CommandDuringShutdown) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(ExpectedWindowCountForBrowserCount(1), CountVisibleWindows());

  chrome::AttemptExit();  // Set chrome::IsTryingToQuit and close all windows.

  // Opening a new window here is fine (unload handlers can also interrupt
  // exit). But closing the window posts an autorelease on
  // BrowserWindowController, which calls ~Browser() and, if that was the last
  // Browser, it invokes applicationWillTerminate: (because IsTryingToQuit is
  // set). So, verify assumptions then process that autorelease.

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(ExpectedWindowCountForBrowserCount(0), CountVisibleWindows());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(ExpectedWindowCountForBrowserCount(0), CountVisibleWindows());

  NSEvent* cmd_n = cocoa_test_event_utils::KeyEventWithKeyCode(
      'n', 'n', NSEventTypeKeyDown, NSEventModifierFlagCommand);
  [[NSApp mainMenu] performSelector:@selector(performKeyEquivalent:)
                         withObject:cmd_n
                         afterDelay:0];
  // Let the run loop get flushed, during process cleanup and try not to crash.
}

class AppControllerKeepAliveBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerKeepAliveBrowserTest() {
    features_.InitAndEnableFeature(features::kDestroyProfileOnBrowserClose);
  }

  base::test::ScopedFeatureList features_;
};

class AppControllerPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppControllerPlatformAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppId,
                                    "1234");
  }

  raw_ptr<const BrowserList> active_browser_list_;
};

// Test that if only a platform app window is open and no browser windows are
// open then a reopen event does nothing.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       DISABLED_PlatformAppReopenWithWindows) {
  NSUInteger old_window_count = NSApp.windows.count;
  EXPECT_EQ(1u, active_browser_list_->size());
  [AppController.sharedController applicationShouldHandleReopen:NSApp
                                              hasVisibleWindows:YES];
  // We do not EXPECT_TRUE the result here because the method
  // deminiaturizes windows manually rather than return YES and have
  // AppKit do it.

  EXPECT_EQ(old_window_count, NSApp.windows.count);
  EXPECT_EQ(1u, active_browser_list_->size());
}

IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       DISABLED_ActivationFocusesBrowserWindow) {
  ExtensionTestMessageListener listener("Launched");
  const extensions::Extension* app =
      InstallAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  NSWindow* app_window = extensions::AppWindowRegistry::Get(profile())
                             ->GetAppWindowsForApp(app->id())
                             .front()
                             ->GetNativeWindow()
                             .GetNativeNSWindow();
  NSWindow* browser_window =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();

  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_LE([NSApp.orderedWindows indexOfObject:app_window],
            [NSApp.orderedWindows indexOfObject:browser_window]);
  [AppController.sharedController applicationShouldHandleReopen:NSApp
                                              hasVisibleWindows:YES];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_LE([NSApp.orderedWindows indexOfObject:browser_window],
            [NSApp.orderedWindows indexOfObject:app_window]);
}

class AppControllerWebAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerWebAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kApp, GetAppURL());
  }

  std::string GetAppURL() const {
    return "http://example.com/";
  }

  raw_ptr<const BrowserList> active_browser_list_;
};

// Test that in web app mode a reopen event opens the app URL.
IN_PROC_BROWSER_TEST_F(AppControllerWebAppBrowserTest,
                       WebAppReopenWithNoWindows) {
  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result =
      [AppController.sharedController applicationShouldHandleReopen:NSApp
                                                  hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, active_browser_list_->size());

  Browser* browser = active_browser_list_->get(0);
  GURL current_url =
      browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(GetAppURL(), current_url.spec());
}

class AppControllerProfilePickerBrowserTest : public InProcessBrowserTest {
 public:
  AppControllerProfilePickerBrowserTest()
      : active_browser_list_(BrowserList::GetInstance()) {}
  ~AppControllerProfilePickerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Flag the profile picker as already shown in the past, to avoid additional
    // feature onboarding logic.
    g_browser_process->local_state()->SetBoolean(
        prefs::kBrowserProfilePickerShown, true);
  }

  const BrowserList* active_browser_list() const {
    return active_browser_list_;
  }

  // Brings the ProfilerPicker onscreen and returns its NSWindow.
  NSWindow* ActivateProfilePicker() {
    NSArray<NSWindow*>* startingWindows = [NSApp windows];

    // ProfilePicker::Show() calls ProfilePicker::Display(), which, for tests,
    // creates the profile asynchronously. Only after the profile gets created
    // is the profile picker initialized and brought onscreen. Therefore, we
    // need to wait for the picker to appear before proceeding with the test.
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileMenuManageProfiles));

    int counter = 5;
    while (!ProfilePicker::IsActive() && counter--) {
      base::TimeDelta delay = base::Seconds(1);
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), delay);
      run_loop.Run();
    }
    EXPECT_TRUE(ProfilePicker::IsActive());

    // The ProfilePicker is the new window in the list.
    for (NSWindow* window in [NSApp windows]) {
      if (![startingWindows containsObject:window]) {
        return window;
      }
    }

    return nil;
  }

 private:
  raw_ptr<const BrowserList> active_browser_list_;
};

// Test that for a guest last profile, commandDispatch should open UserManager
// if guest mode is disabled. Note that this test might be flaky under ASAN
// due to https://crbug.com/674475. Please disable this test under ASAN
// as the tests below if that happened.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       OpenGuestProfileOnlyIfGuestModeIsEnabled) {
  SetGuestProfileAsLastProfile();

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, false);
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  EXPECT_EQ(1u, active_browser_list()->size());

  [app_controller commandDispatch:item];

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();

  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, true);
  [app_controller commandDispatch:item];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, active_browser_list()->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       AboutChromeGuestDisallowed) {
  SetGuestProfileAsLastProfile();

  // Disallow guest by policy and make sure "About Chrome" is not available
  // in the menu.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, false);
  NSMenuItem* about_menu_item = [[[NSApp.mainMenu itemWithTag:IDC_CHROME_MENU]
      submenu] itemWithTag:IDC_ABOUT];
  EXPECT_FALSE([AppController.sharedController
      validateUserInterfaceItem:about_menu_item]);
}

// Test that for a regular last profile, a reopen event opens a browser.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       RegularProfileReopenWithNoWindows) {
  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result =
      [AppController.sharedController applicationShouldHandleReopen:NSApp
                                                  hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, active_browser_list()->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

// Test that for a locked last profile, a reopen event opens the ProfilePicker.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       LockedProfileReopenWithNoWindows) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(true);
  // The User Manager uses the system profile as its underlying profile. To
  // minimize flakiness due to the scheduling/descheduling of tasks on the
  // different threads, pre-initialize the guest profile before it is needed.
  CreateAndWaitForSystemProfile();
  AppController* app_controller = AppController.sharedController;

  // Lock the active profile.
  Profile* profile = [app_controller lastProfileIfLoaded];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  EXPECT_TRUE(entry->IsSigninRequired());

  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result = [app_controller applicationShouldHandleReopen:NSApp
                                            hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
}

// "About Chrome" does not unlock the profile (regression test for
// https://crbug.com/1226844).
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       AboutPanelDoesNotUnlockProfile) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(true);
  // The User Manager uses the system profile as its underlying profile. To
  // minimize flakiness due to the scheduling/descheduling of tasks on the
  // different threads, pre-initialize the guest profile before it is needed.
  CreateAndWaitForSystemProfile();
  AppController* app_controller = AppController.sharedController;
  // Lock the active profile.
  Profile* profile = [app_controller lastProfileIfLoaded];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  EXPECT_TRUE(entry->IsSigninRequired());
  EXPECT_EQ(1u, active_browser_list()->size());
  Browser* browser = active_browser_list()->get(0);
  EXPECT_FALSE(browser->profile()->IsGuestSession());
  // "About Chrome" is not available in the menu.
  NSMenu* chrome_submenu =
      [[NSApp.mainMenu itemWithTag:IDC_CHROME_MENU] submenu];
  NSMenuItem* about_menu_item = [chrome_submenu itemWithTag:IDC_ABOUT];
  EXPECT_FALSE([app_controller validateUserInterfaceItem:about_menu_item]);
  [chrome_submenu update];
  EXPECT_FALSE([about_menu_item isEnabled]);
}

// Test that for a guest last profile, a reopen event opens the ProfilePicker.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       GuestProfileReopenWithNoWindows) {
  SetGuestProfileAsLastProfile();

  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result =
      [AppController.sharedController applicationShouldHandleReopen:NSApp
                                                  hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
}

// Test that the ProfilePicker is shown when there are multiple profiles.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       MultiProfilePickerShown) {
  CreateAndWaitForSystemProfile();

  // Add a profile in the cache (simulate another profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  profile_storage->AddProfile(std::move(params));

  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result =
      [AppController.sharedController applicationShouldHandleReopen:NSApp
                                                  hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
}

// Checks that menu items and commands work when the profile picker is open.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest, MenuCommands) {
  AppController* app_controller = AppController.sharedController;

  // Bring the ProfilePicker onscreen. In normal browser operation, it would
  // be the mainWindow, but with Ventura, the test harness can't activate
  // Chrome, and -mainWindow can return nil. Use a workaround to make it the
  // main window.
  NSWindow* profileWindow = ActivateProfilePicker();
  [app_controller setMainWindowForTesting:profileWindow];

  // Menus are updated before they are brought onscreen. This includes a call
  // to -menuNeedsUpdate: to update the menu's items.
  NSMenu* file_submenu = [[NSApp.mainMenu itemWithTag:IDC_FILE_MENU] submenu];
  [app_controller menuNeedsUpdate:file_submenu];

  // The Profiler Picker has no tabs, so Close Tab should not be present.
  NSMenuItem* close_tab_menu_item = [file_submenu itemWithTag:IDC_CLOSE_TAB];
  EXPECT_EQ(nil, close_tab_menu_item);

  // Close Window should be available.
  NSMenuItem* close_window_menu_item =
      [file_submenu itemWithTag:IDC_CLOSE_WINDOW];
  EXPECT_FALSE([close_window_menu_item isHidden]);
  EXPECT_TRUE([NSApp validateMenuItem:close_window_menu_item]);

  // Make sure New Window works.
  NSMenuItem* new_window_menu_item = [file_submenu itemWithTag:IDC_NEW_WINDOW];
  EXPECT_TRUE([new_window_menu_item isEnabled]);
  EXPECT_TRUE([app_controller validateUserInterfaceItem:new_window_menu_item]);

  // Activate the item and check that a new browser is opened.
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  [file_submenu
      performActionForItemAtIndex:[file_submenu
                                      indexOfItem:new_window_menu_item]];
  EXPECT_TRUE(browser_added_observer.Wait());
}

class AppControllerFirstRunBrowserTest : public AppControllerBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->RemoveSwitch(switches::kNoFirstRun);
  }
};

IN_PROC_BROWSER_TEST_F(AppControllerFirstRunBrowserTest,
                       OpenNewWindowWhileFreIsRunning) {
  EXPECT_TRUE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);

  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [app_controller commandDispatch:item];

  profiles::testing::WaitForPickerClosed();
  EXPECT_FALSE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AppControllerFirstRunBrowserTest,
                       ClickingChromeDockIconDoesNotOpenBrowser) {
  EXPECT_TRUE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  [AppController.sharedController applicationShouldHandleReopen:NSApp
                                              hasVisibleWindows:NO];

  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  ProfilePicker::Hide();
}

class AppControllerOpenShortcutBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerOpenShortcutBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // In order to mimic opening shortcut during browser startup, we need to
    // send the event before -applicationDidFinishLaunching is called, but
    // after AppController is loaded.
    //
    // Since -applicationWillFinishLaunching does nothing now, we swizzle it to
    // our function to send the event. We need to do this early before running
    // the main message loop.
    //
    // NSApp does not exist yet. We need to get the AppController using
    // reflection.
    Class appControllerClass = NSClassFromString(@"AppController");
    Class openShortcutClass = NSClassFromString(@"TestOpenShortcutOnStartup");

    ASSERT_TRUE(appControllerClass != nil);
    ASSERT_TRUE(openShortcutClass != nil);

    SEL targetMethod = @selector(applicationWillFinishLaunching:);
    Method original = class_getInstanceMethod(appControllerClass,
        targetMethod);
    Method destination = class_getInstanceMethod(openShortcutClass,
        targetMethod);

    ASSERT_TRUE(original);
    ASSERT_TRUE(destination);

    method_exchangeImplementations(original, destination);

    ASSERT_TRUE(embedded_test_server()->Start());
    g_open_shortcut_url = embedded_test_server()->GetURL("/simple.html");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // If the arg is empty, PrepareTestCommandLine() after this function will
    // append about:blank as default url.
    command_line->AppendArg(chrome::kChromeUINewTabURL);
  }
};

IN_PROC_BROWSER_TEST_F(AppControllerOpenShortcutBrowserTest,
                       OpenShortcutOnStartup) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  EXPECT_EQ(g_open_shortcut_url,
      browser()->tab_strip_model()->GetActiveWebContents()
          ->GetLastCommittedURL());
}

class AppControllerReplaceNTPBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerReplaceNTPBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // If the arg is empty, PrepareTestCommandLine() after this function will
    // append about:blank as default url.
    command_line->AppendArg(chrome::kChromeUINewTabURL);
  }
};

// Tests that when a GURL is opened after startup, it replaces the NTP.
// Flaky. See crbug.com/1234765.
IN_PROC_BROWSER_TEST_F(AppControllerReplaceNTPBrowserTest,
                       DISABLED_ReplaceNTPAfterStartup) {
  // Depending on network connectivity, the NTP URL can either be
  // chrome://newtab/ or chrome://new-tab-page-third-party. See
  // ntp_test_utils::GetFinalNtpUrl for more details.
  std::string expected_url =
      ntp_test_utils::GetFinalNtpUrl(browser()->profile()).spec();

  // Ensure that there is exactly 1 tab showing, and the tab is the NTP.
  GURL ntp(expected_url);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  browser()->tab_strip_model()->GetActiveWebContents()->GetController().LoadURL(
      GURL(expected_url), content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_LINK, std::string());

  // Wait for one navigation on the active web contents.
  content::TestNavigationObserver ntp_navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ntp_navigation_observer.Wait();

  EXPECT_EQ(ntp,
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());

  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendOpenUrlToAppController(simple);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  content::TestNavigationObserver event_navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  event_navigation_observer.Wait();

  EXPECT_EQ(simple,
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Tests that, even if an incognito browser is the last active browser, a GURL
// is opened in a regular (non-incognito) browser.
// Regression test for https://crbug.com/757253, https://crbug.com/1444747
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest, OpenInRegularBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::apple::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);
  // Create an incognito browser and make it the last active browser.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  EXPECT_EQ(incognito_browser, chrome::FindLastActive());
  // Assure that `windowDidBecomeMain` is called even if this browser process
  // lost focus because of other browser processes in other shards taking
  // focus. It prevents flakiness.
  // See: https://crrev.com/c/4530255/comments/2aadb9cf_9a39d4bf
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:incognito_browser->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(simple);
  event_navigation_observer.Wait();
  // It should be opened in the regular browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(simple, browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

// Tests that, even if only an incognito browser is currently opened, a GURL
// is opened in a regular (non-incognito) browser.
// Regression test for https://crbug.com/757253, https://crbug.com/1444747
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest,
                       OpenInRegularBrowserWhenOnlyIncognitoBrowserIsOpened) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::apple::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Close the current browser.
  Profile* profile = browser()->profile();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  // Create an incognito browser and check that it is the last active browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(incognito_browser, chrome::FindLastActive());
  // Assure that `windowDidBecomeMain` is called even if this browser process
  // lost focus because of other browser processes in other shards taking
  // focus. It prevents flakiness.
  // See: https://crrev.com/c/4530255/comments/2aadb9cf_9a39d4bf
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:incognito_browser->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(simple);
  event_navigation_observer.Wait();
  // Check that a new regular browser is opened
  // and the url is opened in the regular browser.
  Browser* new_browser = chrome::FindLastActive();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_TRUE(new_browser->profile()->IsRegularProfile());
  EXPECT_EQ(profile, new_browser->profile());
  EXPECT_EQ(simple, new_browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

// Tests that, if a guest browser is the last active browser, a GURL is opened
// in the guest browser.
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest, OpenUrlInGuestBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::apple::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);
  // Create a guest browser and make it the last active browser.
  Browser* guest_browser = CreateGuestBrowser();
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, guest_browser->tab_strip_model()->count());
  EXPECT_TRUE(guest_browser->profile()->IsGuestSession());
  guest_browser->window()->Show();
  EXPECT_EQ(guest_browser, chrome::FindLastActive());
  // Assure that `windowDidBecomeMain` is called even if this browser process
  // lost focus because of other browser processes in other shards taking
  // focus. It prevents flakiness.
  // See: https://crrev.com/c/4530255/comments/2aadb9cf_9a39d4bf
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:guest_browser->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(simple);
  event_navigation_observer.Wait();
  // It should be opened in the guest browser.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, guest_browser->tab_strip_model()->count());
  EXPECT_EQ(simple, guest_browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

// Tests that when a GURL is opened while incognito forced and there is no
// browser opened, it is opened in a new incognito browser.
// Test for https://crbug.com/1444747#c8
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest, OpenUrlWhenForcedIncognito) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Close the current non-incognito browser.
  Profile* profile = browser()->profile();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  // Force incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kForced);
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(simple);
  event_navigation_observer.Wait();
  // Check that a new incognito browser is opened
  // and the url is opened in the incognito browser.
  Browser* new_browser = chrome::FindLastActive();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_TRUE(new_browser->profile()->IsIncognitoProfile());
  EXPECT_TRUE(new_browser->profile()->IsPrimaryOTRProfile());
  EXPECT_EQ(profile, new_browser->profile()->GetOriginalProfile());
  EXPECT_EQ(simple, new_browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

// Tests that when a GURL is opened while incognito forced and an incognito
// browser is opened, it is opened in the already opened incognito browser.
// Test for https://crbug.com/1444747#c8
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest,
                       OpenUrlWhenForcedIncognitoAndIncognitoBrowserIsOpened) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Close the current non-incognito browser.
  Profile* profile = browser()->profile();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  // Force incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kForced);
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(incognito_browser, chrome::FindLastActive());
  // Assure that `windowDidBecomeMain` is called even if this browser process
  // lost focus because of other browser processes in other shards taking
  // focus. It prevents flakiness.
  // See: https://crrev.com/c/4530255/comments/2aadb9cf_9a39d4bf
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:incognito_browser->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(simple);
  event_navigation_observer.Wait();
  // Check the url is opened in the already opened incognito browser.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(simple, incognito_browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

class AppControllerShortcutsNotAppsBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerShortcutsNotAppsBrowserTest() {
    features_.InitAndEnableFeature(features::kShortcutsNotApps);
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(AppControllerShortcutsNotAppsBrowserTest,
                       OpenChromeWeblocFile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::apple::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);

  // Create and open a .crwebloc file
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  base::ScopedTempDir temp_dir;
  base::FilePath crwebloc_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    crwebloc_file = temp_dir.GetPath().AppendASCII("test shortcut.crwebloc");
    ASSERT_TRUE(shortcuts::ChromeWeblocFile(
                    simple, *base::SafeBaseName::Create(
                                browser()->profile()->GetPath()))
                    .SaveToFile(crwebloc_file));
  }

  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(net::FilePathToFileURL(crwebloc_file));
  event_navigation_observer.Wait();
  // It should be opened in the regular browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(simple, browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.Delete());
  }
}

IN_PROC_BROWSER_TEST_F(AppControllerShortcutsNotAppsBrowserTest,
                       OpenChromeWeblocFileInSecondProfile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::apple::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);

  // Create profile 2.
  Profile* profile2_ptr = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    profile2_ptr = profile_manager->GetProfile(
        profile_manager->GenerateNextProfileDirectoryPath());
  }

  // Create and open a .crwebloc file
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  base::ScopedTempDir temp_dir;
  base::FilePath crwebloc_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    crwebloc_file = temp_dir.GetPath().AppendASCII("test shortcut.crwebloc");
    ASSERT_TRUE(
        shortcuts::ChromeWeblocFile(
            simple, *base::SafeBaseName::Create(profile2_ptr->GetPath()))
            .SaveToFile(crwebloc_file));
  }

  content::TestNavigationObserver event_navigation_observer(simple);
  event_navigation_observer.StartWatchingNewWebContents();
  SendOpenUrlToAppController(net::FilePathToFileURL(crwebloc_file));
  event_navigation_observer.Wait();

  // It should be opened in a new browser in the second profile.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  Browser* new_browser = chrome::FindLastActive();
  EXPECT_EQ(profile2_ptr, new_browser->profile());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(simple, new_browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.Delete());
  }
}

IN_PROC_BROWSER_TEST_F(AppControllerShortcutsNotAppsBrowserTest,
                       LockedProfileOpensProfilePicker) {
  // Flag the profile picker as already shown in the past, to avoid additional
  // feature onboarding logic.
  g_browser_process->local_state()->SetBoolean(
      prefs::kBrowserProfilePickerShown, true);
  signin_util::ScopedForceSigninSetterForTesting signin_setter(true);
  // The User Manager uses the system profile as its underlying profile. To
  // minimize flakiness due to the scheduling/descheduling of tasks on the
  // different threads, pre-initialize the guest profile before it is needed.
  CreateAndWaitForSystemProfile();
  AppController* app_controller = AppController.sharedController;
  // Lock the active profile.
  Profile* profile = [app_controller lastProfileIfLoaded];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  EXPECT_TRUE(entry->IsSigninRequired());
  // Create and open a .crwebloc file
  GURL simple("https://simple.invalid/");
  base::ScopedTempDir temp_dir;
  base::FilePath crwebloc_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    crwebloc_file = temp_dir.GetPath().AppendASCII("test shortcut.crwebloc");
    ASSERT_TRUE(shortcuts::ChromeWeblocFile(
                    simple, *base::SafeBaseName::Create(profile->GetPath()))
                    .SaveToFile(crwebloc_file));
  }
  SendOpenUrlToAppController(net::FilePathToFileURL(crwebloc_file));
  auto* active_browser_list = BrowserList::GetInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.Delete());
  }
}

class AppControllerMainMenuBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerMainMenuBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
    HistoryMenuResetAfterProfileDeletion) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* app_controller = AppController.sharedController;

  // Use the existing profile as profile 1.
  Profile* profile1 = browser()->profile();

  // Create profile 2.
  base::FilePath profile2_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile2_path);

  // Load profile1's History Service backend so it will be assigned to the
  // HistoryMenuBridge when setLastProfile is called, or else this test will
  // fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile1, ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile1.
  [app_controller setLastProfile:profile1];
  base::RunLoop().RunUntilIdle();

  // Verify the controller's History Menu corresponds to profile1.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
  EXPECT_EQ([app_controller historyMenuBridge]->service(),
            HistoryServiceFactory::GetForProfile(
                profile1, ServiceAccessType::EXPLICIT_ACCESS));

  // Load profile2's History Service backend so it will be assigned to the
  // HistoryMenuBridge when setLastProfile is called, or else this test will
  // fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      &profile2, ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile2.
  [app_controller setLastProfile:&profile2];
  base::RunLoop().RunUntilIdle();

  // Verify the controller's History Menu has changed.
  EXPECT_TRUE([app_controller historyMenuBridge]->service());
  EXPECT_EQ([app_controller historyMenuBridge]->service(),
            HistoryServiceFactory::GetForProfile(
                &profile2, ServiceAccessType::EXPLICIT_ACCESS));
  EXPECT_NE(HistoryServiceFactory::GetForProfile(
                profile1, ServiceAccessType::EXPLICIT_ACCESS),
            HistoryServiceFactory::GetForProfile(
                &profile2, ServiceAccessType::EXPLICIT_ACCESS));

  // Delete profile2.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile2.GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  // Verify the controller's history is back to profile1.
  EXPECT_EQ([app_controller historyMenuBridge]->service(),
            HistoryServiceFactory::GetForProfile(
                profile1, ServiceAccessType::EXPLICIT_ACCESS));
}

// Disabled because of flakiness. See crbug.com/1278031.
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
                       DISABLED_ReloadingDestroyedProfileDoesNotCrash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* app_controller = AppController.sharedController;

  Profile* profile = browser()->profile();
  base::FilePath profile_path = profile->GetPath();

  // Switch the controller to |profile|.
  [app_controller setLastProfile:profile];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(profile, [app_controller lastProfileIfLoaded]);

  // Trigger Profile* destruction. Note that this event (destruction from
  // memory) is a separate event from profile deletion (from disk).
  chrome::CloseAllBrowsers();
  ProfileDestructionWaiter(profile).Wait();
  EXPECT_EQ(nullptr, [app_controller lastProfileIfLoaded]);

  // Re-open the profile. Since the Profile* is destroyed, this involves loading
  // it from disk.
  base::ScopedAllowBlockingForTesting allow_blocking;
  profile = profile_manager->GetProfile(profile_path);
  [app_controller setLastProfile:profile];
  base::RunLoop().RunUntilIdle();

  // We mostly want to make sure re-loading the same profile didn't cause a
  // crash. This means we didn't have e.g. a dangling ProfilePrefRegistrar, or
  // observers pointing to the old (now dead) Profile.
  EXPECT_EQ(profile, [app_controller lastProfileIfLoaded]);
}

IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
    BookmarksMenuIsRestoredAfterProfileSwitch) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* app_controller = AppController.sharedController;

  [app_controller mainMenuCreated];

  // Constants for bookmarks that we will create later.
  const std::u16string title1(u"Dinosaur Comics");
  const GURL url1("http://qwantz.com//");

  const std::u16string title2(u"XKCD");
  const GURL url2("https://www.xkcd.com/");

  // Use the existing profile as profile 1.
  Profile* profile1 = browser()->profile();
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile1));

  // Create profile 2.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path2 = profile_manager->GenerateNextProfileDirectoryPath();
  std::unique_ptr<Profile> profile2 =
      Profile::CreateProfile(path2, nullptr, Profile::CreateMode::kSynchronous);
  Profile* profile2_ptr = profile2.get();
  profile_manager->RegisterTestingProfile(std::move(profile2), false);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile2_ptr));

  // Switch to profile 1, create bookmark 1 and force the menu to build.
  [app_controller setLastProfile:profile1];
  [app_controller bookmarkMenuBridge]->GetBookmarkModel()
      -> AddURL([app_controller bookmarkMenuBridge]->GetBookmarkModel()
                    -> bookmark_bar_node(),
                0, title1, url1);
  NSMenu* profile1_submenu =
      [app_controller bookmarkMenuBridge]->BookmarkMenu();
  [[profile1_submenu delegate] menuNeedsUpdate:profile1_submenu];

  // Switch to profile 2, create bookmark 2 and force the menu to build.
  [app_controller setLastProfile:profile2_ptr];
  [app_controller bookmarkMenuBridge]->GetBookmarkModel()
      -> AddURL([app_controller bookmarkMenuBridge]->GetBookmarkModel()
                    -> bookmark_bar_node(),
                0, title2, url2);
  NSMenu* profile2_submenu =
      [app_controller bookmarkMenuBridge]->BookmarkMenu();
  [[profile2_submenu delegate] menuNeedsUpdate:profile2_submenu];
  EXPECT_NE(profile1_submenu, profile2_submenu);

  // Test that only bookmark 2 is shown.
  EXPECT_FALSE([[app_controller bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title1)]);
  EXPECT_TRUE([[app_controller bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title2)]);

  // Switch *back* to profile 1 and *don't* force the menu to build.
  [app_controller setLastProfile:profile1];

  // Test that only bookmark 1 is shown in the restored menu.
  EXPECT_TRUE([[app_controller bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title1)]);
  EXPECT_FALSE([[app_controller bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title2)]);

  // Ensure a cached menu was used.
  EXPECT_EQ(profile1_submenu,
            [app_controller bookmarkMenuBridge]->BookmarkMenu());
}

// Tests opening a new window from a browser command while incognito is forced.
// Regression test for https://crbug.com/1206726
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
                       ForcedIncognito_NewWindow) {
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  // Close the current non-incognito browser.
  Profile* profile = browser()->profile();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  // Force incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), policy::IncognitoModeAvailability::kForced);
  // Simulate click on "New window".
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [app_controller commandDispatch:item];
  // Check that a new incognito browser is opened.
  Browser* new_browser = browser_added_observer.Wait();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_TRUE(new_browser->profile()->IsPrimaryOTRProfile());
  EXPECT_EQ(profile, new_browser->profile()->GetOriginalProfile());
}

}  // namespace

//--------------------------AppControllerHandoffBrowserTest---------------------

static GURL g_handoff_url;
static std::u16string g_handoff_title;

@interface AppController (BrowserTest)
- (void)new_updateHandoffManagerWithURL:(const GURL&)handoffURL
                                  title:(const std::u16string&)handoffTitle;
@end

@implementation AppController (BrowserTest)
- (void)new_updateHandoffManagerWithURL:(const GURL&)handoffURL
                                  title:(const std::u16string&)handoffTitle {
  g_handoff_url = handoffURL;
  g_handoff_title = handoffTitle;
}
@end

namespace {

class AppControllerHandoffBrowserTest : public InProcessBrowserTest {
 protected:
  // Swizzle Handoff related implementations.
  void SetUpInProcessBrowserTestFixture() override {
    // This swizzle intercepts the URL that would be sent to the Handoff
    // Manager, and instead puts it into a variable accessible to this test.
    swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        [AppController class], @selector(updateHandoffManagerWithURL:title:),
        @selector(new_updateHandoffManagerWithURL:title:));
  }

  void TearDownInProcessBrowserTestFixture() override { swizzler_.reset(); }
  // Closes the tab, and waits for the close to finish.
  void CloseTab(Browser* browser, int index) {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        browser->tab_strip_model()->GetWebContentsAt(index));
    browser->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzler_;
};

// Tests that as a user switches between tabs, navigates within a tab, and
// switches between browser windows, the correct URL is being passed to the
// Handoff.
IN_PROC_BROWSER_TEST_F(AppControllerHandoffBrowserTest, TestHandoffURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(g_handoff_url, GURL(url::kAboutBlankURL));
  EXPECT_EQ(g_handoff_title, u"about:blank");

  // Test that navigating to a URL updates the handoff manager.
  GURL test_url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url1));
  EXPECT_EQ(g_handoff_url, test_url1);
  EXPECT_TRUE(base::EndsWith(g_handoff_title, u"title1.html"));

  // Test that opening a new tab updates the handoff URL.
  GURL test_url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser(), test_url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(g_handoff_url, test_url2);

  // Test that switching tabs updates the handoff URL.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(g_handoff_url, test_url1);
  EXPECT_TRUE(base::EndsWith(g_handoff_title, u"title1.html"));

  // Test that closing the current tab updates the handoff URL.
  CloseTab(browser(), 0);
  EXPECT_EQ(g_handoff_url, test_url2);
  EXPECT_EQ(g_handoff_title, u"Title Of Awesomeness");

  // Test that opening a new browser window updates the handoff URL.
  GURL test_url3 = embedded_test_server()->GetURL("/title3.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(test_url3), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(g_handoff_url, test_url3);
  EXPECT_EQ(g_handoff_title, u"Title Of More Awesomeness");

  // Check that there are exactly 2 browsers.
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the second browser window (which only has 1 tab left).
  Browser* browser2 = active_browser_list->get(1);
  CloseBrowserSynchronously(browser2);
  EXPECT_EQ(g_handoff_url, test_url2);
  EXPECT_EQ(g_handoff_title, u"Title Of Awesomeness");

  // The URLs of incognito windows should not be passed to Handoff.
  GURL test_url4 = embedded_test_server()->GetURL("/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(test_url4), WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(g_handoff_url, GURL());
  EXPECT_EQ(g_handoff_title, u"");

  // Open a new tab in the incognito window.
  EXPECT_EQ(2u, active_browser_list->size());
  Browser* browser3 = active_browser_list->get(1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser3, test_url4, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  EXPECT_EQ(g_handoff_url, GURL());
  EXPECT_EQ(g_handoff_title, u"");

  // Navigate the current tab in the incognito window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser3, test_url1));
  EXPECT_EQ(g_handoff_url, GURL());
  EXPECT_EQ(g_handoff_title, u"");

  // Activate the original browser window.
  Browser* browser1 = active_browser_list->get(0);
  browser1->window()->Show();
  EXPECT_EQ(g_handoff_url, test_url2);
  EXPECT_EQ(g_handoff_title, u"Title Of Awesomeness");
}

class AppControllerHandoffPrerenderBrowserTest
    : public AppControllerHandoffBrowserTest {
 public:
  void SetUpOnMainThread() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 protected:
  AppControllerHandoffPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &AppControllerHandoffPrerenderBrowserTest::GetActiveWebContents,
            // Unretained is safe here, as this class owns PrerenderTestHelper
            // object, which holds the callback being constructed here, so the
            // callback will be destructed before this class.
            base::Unretained(this))) {}

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that as a user switches from main page to prerendered page, the correct
// URL is being passed to the Handoff.
IN_PROC_BROWSER_TEST_F(AppControllerHandoffPrerenderBrowserTest,
                       TestHandoffURLs) {
  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);
  EXPECT_EQ(g_handoff_url, url);

  // Activate.
  content::TestActivationManager navigation_manager(GetActiveWebContents(),
                                                    prerender_url);
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", prerender_url)));
  navigation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(navigation_manager.was_activated());
  EXPECT_TRUE(navigation_manager.was_successful());
  EXPECT_EQ(g_handoff_url, prerender_url);
}

}  // namespace
