// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
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
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
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
#include "components/prefs/pref_service.h"
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
#include "net/base/mac/url_conversions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

GURL g_open_shortcut_url = GURL::EmptyGURL();

// Instructs the NSApp's delegate to open |url|.
void SendOpenUrlToAppController(const GURL& url) {
  [NSApp.delegate application:NSApp openURLs:@[ net::NSURLWithGURL(url) ]];
}

Profile* CreateAndWaitForProfile(const base::FilePath& profile_dir) {
  Profile* profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(), profile_dir);
  EXPECT_TRUE(profile);
  return profile;
}

Profile* CreateAndWaitForSystemProfile() {
  return CreateAndWaitForProfile(ProfileManager::GetSystemProfilePath());
}

Profile* CreateAndWaitForGuestProfile() {
  return CreateAndWaitForProfile(ProfileManager::GetGuestProfilePath());
}

void SetGuestProfileAsLastProfile() {
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  // Create the guest profile, and set it as the last used profile.
  Profile* guest_profile = CreateAndWaitForGuestProfile();
  [ac setLastProfile:guest_profile];

  Profile* profile = [ac lastProfileIfLoaded];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile->GetPath(), profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());

  // Also set the last used profile path preference. If the profile does need to
  // be read from disk for some reason this acts as a backstop.
  g_browser_process->local_state()->SetString(
      prefs::kProfileLastUsed, guest_profile->GetPath().BaseName().value());
}

// Key for ProfileDestroyedData user data.
const char kProfileDestrictionWaiterUserDataKey = 0;

// Waits until the Profile instance is destroyed.
class ProfileDestructionWaiter {
 public:
  explicit ProfileDestructionWaiter(Profile* profile) {
    profile->SetUserData(
        &kProfileDestrictionWaiterUserDataKey,
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

// Regression test for https://crbug.com/1236073
// TODO(crbug.com/1373692): Extremely flaky on the mac12-arm64-rel bot.
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest,
                       DISABLED_DeleteEphemeralProfile) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  Profile* profile = browser()->profile();
  // Activate the first profile.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:browser()
                               ->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  ASSERT_EQ(profile, [ac lastProfileIfLoaded]);

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
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Create a new profile and activate it.
  Profile* profile2 = CreateAndWaitForProfile(
      profile_manager->user_data_dir().AppendASCII("Profile 2"));
  Browser* browser2 = CreateBrowser(profile2);
  // This should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:browser2->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  ASSERT_EQ(profile2, [ac lastProfileIfLoaded]);
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
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  NSUInteger old_window_count = [[NSApp windows] count];
  EXPECT_EQ(1u, active_browser_list_->size());
  [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:YES];
  // We do not EXPECT_TRUE the result here because the method
  // deminiaturizes windows manually rather than return YES and have
  // AppKit do it.

  EXPECT_EQ(old_window_count, [[NSApp windows] count]);
  EXPECT_EQ(1u, active_browser_list_->size());
}

IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       DISABLED_ActivationFocusesBrowserWindow) {
  AppController* app_controller = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(app_controller);

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
  EXPECT_LE([[NSApp orderedWindows] indexOfObject:app_window],
            [[NSApp orderedWindows] indexOfObject:browser_window]);
  [app_controller applicationShouldHandleReopen:NSApp
                              hasVisibleWindows:YES];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_LE([[NSApp orderedWindows] indexOfObject:browser_window],
            [[NSApp orderedWindows] indexOfObject:app_window]);
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
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

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
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  EXPECT_EQ(1u, active_browser_list()->size());

  [ac commandDispatch:item];

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();

  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, true);
  [ac commandDispatch:item];
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
  base::scoped_nsobject<NSMenuItem> about_menu_item(
      [[[[NSApp mainMenu] itemWithTag:IDC_CHROME_MENU] submenu]
          itemWithTag:IDC_ABOUT],
      base::scoped_policy::RETAIN);
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  EXPECT_FALSE([ac validateUserInterfaceItem:about_menu_item]);
}

// Test that for a regular last profile, a reopen event opens a browser.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       RegularProfileReopenWithNoWindows) {
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

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
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  // Lock the active profile.
  Profile* profile = [ac lastProfileIfLoaded];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  EXPECT_TRUE(entry->IsSigninRequired());

  EXPECT_EQ(1u, active_browser_list()->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
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
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  // Lock the active profile.
  Profile* profile = [ac lastProfileIfLoaded];
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
  base::scoped_nsobject<NSMenu> chrome_submenu(
      [[[NSApp mainMenu] itemWithTag:IDC_CHROME_MENU] submenu],
      base::scoped_policy::RETAIN);
  base::scoped_nsobject<NSMenuItem> about_menu_item(
      [chrome_submenu itemWithTag:IDC_ABOUT], base::scoped_policy::RETAIN);
  EXPECT_FALSE([ac validateUserInterfaceItem:about_menu_item]);
  [chrome_submenu update];
  EXPECT_FALSE([about_menu_item isEnabled]);
}

// Test that for a guest last profile, a reopen event opens the ProfilePicker.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       GuestProfileReopenWithNoWindows) {
  SetGuestProfileAsLastProfile();

  EXPECT_EQ(1u, active_browser_list()->size());
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
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
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

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
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, active_browser_list()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
}

// Checks that menu items and commands work when the profile picker is open.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest, MenuCommands) {
  // Show the profile picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));

  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  // Unhandled menu items are disabled.
  base::scoped_nsobject<NSMenu> file_submenu(
      [[[NSApp mainMenu] itemWithTag:IDC_FILE_MENU] submenu],
      base::scoped_policy::RETAIN);
  base::scoped_nsobject<NSMenuItem> close_tab_menu_item(
      [file_submenu itemWithTag:IDC_CLOSE_TAB], base::scoped_policy::RETAIN);
  EXPECT_FALSE([ac validateUserInterfaceItem:close_tab_menu_item]);
  [file_submenu update];
  EXPECT_FALSE([close_tab_menu_item isEnabled]);

  // Enabled menu items work.
  base::scoped_nsobject<NSMenuItem> new_window_menu_item(
      [file_submenu itemWithTag:IDC_NEW_WINDOW], base::scoped_policy::RETAIN);
  EXPECT_TRUE([new_window_menu_item isEnabled]);
  EXPECT_TRUE([ac validateUserInterfaceItem:new_window_menu_item]);
  // Click on the item and checks that a new browser is opened.
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  [file_submenu
      performActionForItemAtIndex:[file_submenu
                                      indexOfItemWithTag:IDC_NEW_WINDOW]];
  EXPECT_TRUE(browser_added_observer.Wait());
}

class AppControllerFirstRunBrowserTest : public AppControllerBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->RemoveSwitch(switches::kNoFirstRun);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};
};

IN_PROC_BROWSER_TEST_F(AppControllerFirstRunBrowserTest,
                       OpenNewWindowWhileFreIsRunning) {
  EXPECT_TRUE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);

  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [ac commandDispatch:item];

  profiles::testing::WaitForPickerClosed();
  EXPECT_FALSE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AppControllerFirstRunBrowserTest,
                       ClickingChromeDockIconDoesNotOpenBrowser) {
  EXPECT_TRUE(ProfilePicker::IsFirstRunOpen());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  ProfilePicker::Hide();
}

class AppControllerOpenShortcutBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerOpenShortcutBrowserTest()
      : AppControllerOpenShortcutBrowserTest(/*enable_fre=*/false) {}

  AppControllerOpenShortcutBrowserTest(bool enable_fre) {
    std::vector<base::test::FeatureRef> enabled_features = {
        welcome::kForceEnabled};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (enable_fre) {
      enabled_features.push_back(kForYouFre);
    } else {
      disabled_features.push_back(kForYouFre);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppControllerOpenShortcutBrowserTest,
                       OpenShortcutOnStartup) {
  // The two tabs expected are the Welcome page and the desired URL.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(g_open_shortcut_url, browser()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetLastCommittedURL());
}

class AppControllerOpenShortcutWithFreBrowserTest
    : public AppControllerOpenShortcutBrowserTest {
 protected:
  AppControllerOpenShortcutWithFreBrowserTest()
      : AppControllerOpenShortcutBrowserTest(/*enable_fre=*/true) {}
};

IN_PROC_BROWSER_TEST_F(AppControllerOpenShortcutWithFreBrowserTest,
                       OpenShortcutOnStartup) {
  // The Welcome page is not expected.
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

// Tests that when a GURL is opened, it is not opened in incognito mode.
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest,
                       DISABLED_OpenInRegularBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(incognito_browser, chrome::GetLastActiveBrowser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendOpenUrlToAppController(simple);
  // It should be opened in the regular browser.
  content::TestNavigationObserver event_navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  event_navigation_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(simple, browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL());
}

class AppControllerMainMenuBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerMainMenuBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
    HistoryMenuResetAfterProfileDeletion) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* ac =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);

  // Use the existing profile as profile 1.
  Profile* profile1 = browser()->profile();

  // Create profile 2.
  base::FilePath profile2_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile* profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile2_path);
  ASSERT_TRUE(profile2);

  // Load profile1's History Service backend so it will be assigned to the
  // HistoryMenuBridge when setLastProfile is called, or else this test will
  // fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile1, ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile1.
  [ac setLastProfile:profile1];
  base::RunLoop().RunUntilIdle();

  // Verify the controller's History Menu corresponds to profile1.
  EXPECT_TRUE([ac historyMenuBridge]->service());
  EXPECT_EQ([ac historyMenuBridge]->service(),
      HistoryServiceFactory::GetForProfile(profile1,
                                           ServiceAccessType::EXPLICIT_ACCESS));

  // Load profile2's History Service backend so it will be assigned to the
  // HistoryMenuBridge when setLastProfile is called, or else this test will
  // fail flaky.
  ui_test_utils::WaitForHistoryToLoad(
      HistoryServiceFactory::GetForProfile(profile2,
                                           ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile2.
  [ac setLastProfile:profile2];
  base::RunLoop().RunUntilIdle();

  // Verify the controller's History Menu has changed.
  EXPECT_TRUE([ac historyMenuBridge]->service());
  EXPECT_EQ([ac historyMenuBridge]->service(),
      HistoryServiceFactory::GetForProfile(profile2,
                                           ServiceAccessType::EXPLICIT_ACCESS));
  EXPECT_NE(
      HistoryServiceFactory::GetForProfile(profile1,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HistoryServiceFactory::GetForProfile(profile2,
                                           ServiceAccessType::EXPLICIT_ACCESS));

  // Delete profile2.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile2->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  // Verify the controller's history is back to profile1.
  EXPECT_EQ([ac historyMenuBridge]->service(),
      HistoryServiceFactory::GetForProfile(profile1,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}

// Disabled because of flakiness. See crbug.com/1278031.
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
                       DISABLED_ReloadingDestroyedProfileDoesNotCrash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* ac =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);

  Profile* profile = browser()->profile();
  base::FilePath profile_path = profile->GetPath();

  // Switch the controller to |profile|.
  [ac setLastProfile:profile];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(profile, [ac lastProfileIfLoaded]);

  // Trigger Profile* destruction. Note that this event (destruction from
  // memory) is a separate event from profile deletion (from disk).
  chrome::CloseAllBrowsers();
  ProfileDestructionWaiter(profile).Wait();
  EXPECT_EQ(nullptr, [ac lastProfileIfLoaded]);

  // Re-open the profile. Since the Profile* is destroyed, this involves loading
  // it from disk.
  base::ScopedAllowBlockingForTesting allow_blocking;
  profile = profile_manager->GetProfile(profile_path);
  [ac setLastProfile:profile];
  base::RunLoop().RunUntilIdle();

  // We mostly want to make sure re-loading the same profile didn't cause a
  // crash. This means we didn't have e.g. a dangling ProfilePrefRegistrar, or
  // observers pointing to the old (now dead) Profile.
  EXPECT_EQ(profile, [ac lastProfileIfLoaded]);
}

IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
    BookmarksMenuIsRestoredAfterProfileSwitch) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  [ac mainMenuCreated];

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
      Profile::CreateProfile(path2, nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  Profile* profile2_ptr = profile2.get();
  profile_manager->RegisterTestingProfile(std::move(profile2), false);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile2_ptr));

  // Switch to profile 1, create bookmark 1 and force the menu to build.
  [ac setLastProfile:profile1];
  [ac bookmarkMenuBridge]->GetBookmarkModel()->AddURL(
      [ac bookmarkMenuBridge]->GetBookmarkModel()->bookmark_bar_node(),
      0, title1, url1);
  NSMenu* profile1_submenu = [ac bookmarkMenuBridge]->BookmarkMenu();
  [[profile1_submenu delegate] menuNeedsUpdate:profile1_submenu];

  // Switch to profile 2, create bookmark 2 and force the menu to build.
  [ac setLastProfile:profile2_ptr];
  [ac bookmarkMenuBridge]->GetBookmarkModel()->AddURL(
      [ac bookmarkMenuBridge]->GetBookmarkModel()->bookmark_bar_node(),
      0, title2, url2);
  NSMenu* profile2_submenu = [ac bookmarkMenuBridge]->BookmarkMenu();
  [[profile2_submenu delegate] menuNeedsUpdate:profile2_submenu];
  EXPECT_NE(profile1_submenu, profile2_submenu);

  // Test that only bookmark 2 is shown.
  EXPECT_FALSE([[ac bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title1)]);
  EXPECT_TRUE([[ac bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title2)]);

  // Switch *back* to profile 1 and *don't* force the menu to build.
  [ac setLastProfile:profile1];

  // Test that only bookmark 1 is shown in the restored menu.
  EXPECT_TRUE([[ac bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title1)]);
  EXPECT_FALSE([[ac bookmarkMenuBridge]->BookmarkMenu()
      itemWithTitle:base::SysUTF16ToNSString(title2)]);

  // Ensure a cached menu was used.
  EXPECT_EQ(profile1_submenu, [ac bookmarkMenuBridge]->BookmarkMenu());
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
      profile->GetPrefs(), IncognitoModePrefs::Availability::kForced);
  // Simulate click on "New window".
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [ac commandDispatch:item];
  // Check that a new incognito browser is opened.
  Browser* new_browser = browser_added_observer.Wait();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_TRUE(new_browser->profile()->IsPrimaryOTRProfile());
  EXPECT_EQ(profile, new_browser->profile()->GetOriginalProfile());
}

// Tests opening a new window from dock menu while incognito browser is opened.
// Regression test for https://crbug.com/1371923
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
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
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  ASSERT_TRUE(menu);
  NSMenuItem* item = [menu itemWithTag:IDC_NEW_WINDOW];
  ASSERT_TRUE(item);
  [ac commandDispatch:item];

  // Check that a new non-incognito browser is opened.
  Browser* new_browser = browser_added_observer.Wait();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_TRUE(new_browser->profile()->IsRegularProfile());
  EXPECT_EQ(profile, new_browser->profile());
}

// Test switching from Regular to OTR profiles updates the history menu.
IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
                       SwitchToIncognitoRemovesHistoryItems) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppController* ac =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);

  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendOpenUrlToAppController(simple);

  Profile* profile = browser()->profile();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  // Load profile's History Service backend so it will be assigned to the
  // HistoryMenuBridge, or else this test will fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Verify that history bridge service is available for regular profiles.
  EXPECT_TRUE([ac historyMenuBridge]->service());

  // Open a URL in Incognito window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), simple, WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  // Check that there are exactly 2 browsers (regular and incognito).
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  // Verify that history beidge service is not available in Incognito.
  EXPECT_FALSE([ac historyMenuBridge]->service());

  // Switch back to the regular profile window.
  Browser* browser1 = active_browser_list->get(0);
  browser1->window()->Show();

  // Verify that history bridge service is available again.
  EXPECT_TRUE([ac historyMenuBridge]->service());
}

class AppControllerIncognitoSwitchTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

// Regression test for https://crbug.com/1248661
IN_PROC_BROWSER_TEST_F(AppControllerIncognitoSwitchTest,
                       ObserveProfileDestruction) {
  // Chrome is launched in incognito.
  Profile* otr_profile = browser()->profile();
  EXPECT_EQ(otr_profile,
            otr_profile->GetPrimaryOTRProfile(/*create_if_needed=*/false));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  AppController* ac =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:browser()
                               ->window()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()];
  // The last profile is the incognito profile.
  EXPECT_EQ([ac lastProfileIfLoaded], otr_profile);
  // Destroy the incognito profile.
  ProfileDestructionWaiter waiter(otr_profile);
  CloseBrowserSynchronously(browser());
  waiter.Wait();
  // Check that |-lastProfileIfLoaded| is not pointing to released memory.
  EXPECT_NE([ac lastProfileIfLoaded], otr_profile);
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
    swizzler_ = std::make_unique<base::mac::ScopedObjCClassSwizzler>(
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
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> swizzler_;
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
    prerender_helper_.SetUp(embedded_test_server());
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
