// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <objc/message.h>
#import <objc/runtime.h>
#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
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
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
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
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/web_apps/web_app_url_handler_intent_picker_dialog_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/url_handler_manager.h"
#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

using base::SysUTF16ToNSString;

@interface AppController (ForTesting)
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
@end

namespace {

GURL g_open_shortcut_url = GURL::EmptyGURL();
const char16_t kAppName[] = u"Test App";
const char kStartUrl[] = "https://test.com";

// Returns an Apple Event that instructs the application to open |url|.
NSAppleEventDescriptor* AppleEventToOpenUrl(const GURL& url) {
  NSAppleEventDescriptor* shortcut_event = [[[NSAppleEventDescriptor alloc]
      initWithEventClass:kASAppleScriptSuite
                 eventID:kASSubroutineEvent
        targetDescriptor:nil
                returnID:kAutoGenerateReturnID
           transactionID:kAnyTransactionID] autorelease];
  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  [shortcut_event setParamDescriptor:[NSAppleEventDescriptor
                                         descriptorWithString:url_string]
                          forKeyword:keyDirectObject];
  return shortcut_event;
}

// Instructs the NSApp's delegate to open |url|.
void SendAppleEventToOpenUrlToAppController(const GURL& url) {
  AppController* controller =
      base::mac::ObjCCast<AppController>([NSApp delegate]);
  [controller getUrl:AppleEventToOpenUrl(url) withReply:nullptr];
}

void RunClosureWhenProfileInitialized(const base::RepeatingClosure& closure,
                                      Profile* profile,
                                      Profile::CreateStatus status) {
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    closure.Run();
}

// Called when the ProfileManager has created a profile.
void CreateProfileCallback(const base::RepeatingClosure& quit_closure,
                           Profile** out_profile,
                           Profile* profile,
                           Profile::CreateStatus status) {
  EXPECT_TRUE(profile);
  ASSERT_TRUE(out_profile);
  *out_profile = profile;
  EXPECT_NE(Profile::CREATE_STATUS_LOCAL_FAIL, status);
  EXPECT_NE(Profile::CREATE_STATUS_REMOTE_FAIL, status);
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    quit_closure.Run();
}

Profile* CreateAndWaitForProfile(const base::FilePath& profile_dir) {
  Profile* profile;
  ProfileManager::CreateCallback create_callback = base::BindRepeating(
      &CreateProfileCallback,
      base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(), &profile);
  g_browser_process->profile_manager()->CreateProfileAsync(profile_dir,
                                                           create_callback);
  base::RunLoop().Run();
  return profile;
}

Profile* CreateAndWaitForSystemProfile() {
  return CreateAndWaitForProfile(ProfileManager::GetSystemProfilePath());
}

void AutoCloseDialog(views::Widget* widget) {
  // Call CancelDialog to close the dialog, but the actual behavior will be
  // determined by the ScopedTestDialogAutoConfirm configs.
  views::test::CancelDialog(widget);
}

}  // namespace

@interface TestOpenShortcutOnStartup : NSObject
- (void)applicationWillFinishLaunching:(NSNotification*)notification;
@end

@implementation TestOpenShortcutOnStartup

- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  if (!g_open_shortcut_url.is_valid())
    return;

  SendAppleEventToOpenUrlToAppController(g_open_shortcut_url);
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
      'n', 'n', NSKeyDown, NSCommandKeyMask);
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

IN_PROC_BROWSER_TEST_F(AppControllerKeepAliveBrowserTest,
                       LastProfileKeepAlive) {
  // The User Manager uses the system profile as its underlying profile. To
  // minimize flakiness due to the scheduling/descheduling of tasks on the
  // different threads, pre-initialize the guest profile before it is needed.
  CreateAndWaitForSystemProfile();
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  // Switch the controller to profile1.
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateAndWaitForProfile(
      profile_manager->user_data_dir().AppendASCII("Profile 2"));
  [ac windowChangedToProfile:profile1];
  ASSERT_EQ(profile1, [ac lastProfile]);

  // |profile1| is active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      profile1, ProfileKeepAliveOrigin::kAppControllerMac));
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      profile2, ProfileKeepAliveOrigin::kAppControllerMac));

  // Make |profile2| active.
  [ac windowChangedToProfile:profile2];
  ASSERT_EQ(profile2, [ac lastProfile]);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      profile1, ProfileKeepAliveOrigin::kAppControllerMac));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      profile2, ProfileKeepAliveOrigin::kAppControllerMac));
}

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

  const BrowserList* active_browser_list_;
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

  ExtensionTestMessageListener listener("Launched", false);
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

  const BrowserList* active_browser_list_;
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
  const BrowserList* active_browser_list_;
};

// Test that for a guest last profile, commandDispatch should open UserManager
// if guest mode is disabled. Note that this test might be flaky under ASAN
// due to https://crbug.com/674475. Please disable this test under ASAN
// as the tests below if that happened.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       OpenGuestProfileOnlyIfGuestModeIsEnabled) {
  CreateAndWaitForSystemProfile();
  base::FilePath guest_profile_path = ProfileManager::GetGuestProfilePath();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         guest_profile_path.BaseName().value());
  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, false);

  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);

  Profile* profile = [ac lastProfile];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile_path, profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());

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
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  // Create the guest profile, and set it as the last used profile so the
  // app controller can use it on init.
  CreateAndWaitForSystemProfile();
  base::FilePath guest_profile_path = ProfileManager::GetGuestProfilePath();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         guest_profile_path.BaseName().value());
  // Disallow guest by policy.
  local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, false);
  Profile* profile = [ac lastProfile];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile_path, profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());
  // "About Chrome" is not available in the menu.
  base::scoped_nsobject<NSMenuItem> about_menu_item(
      [[[[NSApp mainMenu] itemWithTag:IDC_CHROME_MENU] submenu]
          itemWithTag:IDC_ABOUT],
      base::scoped_policy::RETAIN);
  EXPECT_FALSE([ac validateUserInterfaceItem:about_menu_item]);
}

// Test that for a regular last profile, a reopen event opens a browser.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       RegularProfileReopenWithNoWindows) {
  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);

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

  // Lock the active profile.
  Profile* profile = [ac lastProfile];
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
  // Lock the active profile.
  Profile* profile = [ac lastProfile];
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
  base::scoped_nsobject<NSMenuItem> about_menu_item(
      [[[[NSApp mainMenu] itemWithTag:IDC_CHROME_MENU] submenu]
          itemWithTag:IDC_ABOUT],
      base::scoped_policy::RETAIN);
  EXPECT_FALSE([ac validateUserInterfaceItem:about_menu_item]);
}

// Test that for a guest last profile, a reopen event opens the ProfilePicker.
IN_PROC_BROWSER_TEST_F(AppControllerProfilePickerBrowserTest,
                       GuestProfileReopenWithNoWindows) {
  // Create the system profile. Set the guest as the last used profile so the
  // app controller can use it on init.
  CreateAndWaitForSystemProfile();
  base::FilePath guest_profile_path = ProfileManager::GetGuestProfilePath();
  g_browser_process->local_state()->SetString(
      prefs::kProfileLastUsed, guest_profile_path.BaseName().value());

  AppController* ac = base::mac::ObjCCastStrict<AppController>(
      [[NSApplication sharedApplication] delegate]);

  Profile* profile = [ac lastProfile];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile_path, profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());

  EXPECT_EQ(1u, active_browser_list()->size());
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

class AppControllerOpenShortcutBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerOpenShortcutBrowserTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
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

    ASSERT_TRUE(original != NULL);
    ASSERT_TRUE(destination != NULL);

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
  EXPECT_EQ(g_open_shortcut_url,
      browser()->tab_strip_model()->GetActiveWebContents()
          ->GetLastCommittedURL());
}

class AppControllerReplaceNTPBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerReplaceNTPBrowserTest() {}

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
IN_PROC_BROWSER_TEST_F(AppControllerReplaceNTPBrowserTest,
                       ReplaceNTPAfterStartup) {
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
  SendAppleEventToOpenUrlToAppController(simple);

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
IN_PROC_BROWSER_TEST_F(AppControllerBrowserTest, OpenInRegularBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(incognito_browser, chrome::GetLastActiveBrowser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());
  // Open a url.
  GURL simple(embedded_test_server()->GetURL("/simple.html"));
  SendAppleEventToOpenUrlToAppController(simple);
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
  AppControllerMainMenuBrowserTest() {
  }
};

IN_PROC_BROWSER_TEST_F(AppControllerMainMenuBrowserTest,
    HistoryMenuResetAfterProfileDeletion) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  AppController* ac =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);

  // Use the existing profile as profile 1.
  Profile* profile1 = browser()->profile();

  // Create profile 2.
  base::FilePath profile2_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      profile2_path, base::BindRepeating(&RunClosureWhenProfileInitialized,
                                         run_loop.QuitClosure()));
  run_loop.Run();
  Profile* profile2 = profile_manager->GetProfileByPath(profile2_path);
  ASSERT_TRUE(profile2);

  // Load profile1's History Service backend so it will be assigned to the
  // HistoryMenuBridge when windowChangedToProfile is called, or else this test
  // will fail flaky.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile1, ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile1.
  [ac windowChangedToProfile:profile1];
  base::RunLoop().RunUntilIdle();

  // Verify the controller's History Menu corresponds to profile1.
  EXPECT_TRUE([ac historyMenuBridge]->service());
  EXPECT_EQ([ac historyMenuBridge]->service(),
      HistoryServiceFactory::GetForProfile(profile1,
                                           ServiceAccessType::EXPLICIT_ACCESS));

  // Load profile2's History Service backend so it will be assigned to the
  // HistoryMenuBridge when windowChangedToProfile is called, or else this test
  // will fail flaky.
  ui_test_utils::WaitForHistoryToLoad(
      HistoryServiceFactory::GetForProfile(profile2,
                                           ServiceAccessType::EXPLICIT_ACCESS));
  // Switch the controller to profile2.
  [ac windowChangedToProfile:profile2];
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
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              base::DoNothing());
  content::RunAllTasksUntilIdle();

  // Verify the controller's history is back to profile1.
  EXPECT_EQ([ac historyMenuBridge]->service(),
      HistoryServiceFactory::GetForProfile(profile1,
                                           ServiceAccessType::EXPLICIT_ACCESS));
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
      Profile::CreateProfile(path2, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  Profile* profile2_ptr = profile2.get();
  profile_manager->RegisterTestingProfile(std::move(profile2), false);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile2_ptr));

  // Switch to profile 1, create bookmark 1 and force the menu to build.
  [ac windowChangedToProfile:profile1];
  [ac bookmarkMenuBridge]->GetBookmarkModel()->AddURL(
      [ac bookmarkMenuBridge]->GetBookmarkModel()->bookmark_bar_node(),
      0, title1, url1);
  NSMenu* profile1_submenu = [ac bookmarkMenuBridge]->BookmarkMenu();
  [[profile1_submenu delegate] menuNeedsUpdate:profile1_submenu];

  // Switch to profile 2, create bookmark 2 and force the menu to build.
  [ac windowChangedToProfile:profile2_ptr];
  [ac bookmarkMenuBridge]->GetBookmarkModel()->AddURL(
      [ac bookmarkMenuBridge]->GetBookmarkModel()->bookmark_bar_node(),
      0, title2, url2);
  NSMenu* profile2_submenu = [ac bookmarkMenuBridge]->BookmarkMenu();
  [[profile2_submenu delegate] menuNeedsUpdate:profile2_submenu];
  EXPECT_NE(profile1_submenu, profile2_submenu);

  // Test that only bookmark 2 is shown.
  EXPECT_FALSE([[ac bookmarkMenuBridge]->BookmarkMenu() itemWithTitle:
      SysUTF16ToNSString(title1)]);
  EXPECT_TRUE([[ac bookmarkMenuBridge]->BookmarkMenu() itemWithTitle:
      SysUTF16ToNSString(title2)]);

  // Switch *back* to profile 1 and *don't* force the menu to build.
  [ac windowChangedToProfile:profile1];

  // Test that only bookmark 1 is shown in the restored menu.
  EXPECT_TRUE([[ac bookmarkMenuBridge]->BookmarkMenu() itemWithTitle:
      SysUTF16ToNSString(title1)]);
  EXPECT_FALSE([[ac bookmarkMenuBridge]->BookmarkMenu() itemWithTitle:
      SysUTF16ToNSString(title2)]);

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
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
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

class StartupWebAppUrlHandlingBrowserTest : public InProcessBrowserTest {
 protected:
  StartupWebAppUrlHandlingBrowserTest()
      : test_web_app_provider_creator_(base::BindRepeating(
            &StartupWebAppUrlHandlingBrowserTest::CreateTestWebAppProvider)) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableUrlHandlers);
  }

  web_app::AppId InstallWebAppWithUrlHandlers(
      const std::vector<apps::UrlHandlerInfo>& url_handlers) {
    return web_app::test::InstallWebAppWithUrlHandlers(
        browser()->profile(), GURL(kStartUrl), kAppName, url_handlers);
  }

  // Check that there are two browsers. Find the one that is not |browser|.
  Browser* FindOneOtherBrowser(Browser* browser) {
    // There should only be one other browser.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

    // Find the new browser.
    Browser* other_browser = nullptr;
    for (auto* b : *BrowserList::GetInstance()) {
      if (b != browser)
        other_browser = b;
    }
    return other_browser;
  }

 private:
  static std::unique_ptr<KeyedService> CreateTestWebAppProvider(
      Profile* profile) {
    auto provider = std::make_unique<web_app::TestWebAppProvider>(profile);
    provider->Start();
    auto association_manager =
        std::make_unique<web_app::FakeWebAppOriginAssociationManager>();
    association_manager->set_pass_through(true);
    auto& url_handler_manager =
        provider->os_integration_manager().url_handler_manager_for_testing();
    url_handler_manager.SetAssociationManagerForTesting(
        std::move(association_manager));
    return provider;
  }

  web_app::TestWebAppProviderCreator test_web_app_provider_creator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogCancelled_NoLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Start URL is in app scope.
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  // When dialog is closed, nothing will happen.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogAccepted_BrowserLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the first choice, which is the browser.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 0);
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  // Check the link of the new tab that was opened.
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(1);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogAccepted_RememberBrowserLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  base::HistogramTester histogram_tester;

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Get matches before dialog launch.
  auto url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(GURL(kStartUrl));

  // Select and remember the first choice, which is the browser.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION, 0);
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kBrowserAcceptedAndRememberChoice,
      1);

  // When dialog is closed, URL will be launched in a browser tab.
  // Check for new tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  // Check the link of the new tab that was opened.
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(1);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());

  // Get matches after dialog is closed.
  auto new_url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(GURL(kStartUrl));
  ASSERT_NE(url_handler_matches, new_url_handler_matches);
  // Verify opening in browser is saved as the default choice (i.e. no matches
  // found).
  ASSERT_TRUE(new_url_handler_matches.empty());

  // Start with the same URL again. A new tab should be opened directly.
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  // Verify a new tab is launched.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_strip->count());
  // Check the link of the new tab that was opened.
  web_contents = tab_strip->GetWebContentsAt(2);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());

  // Dialog wasn't shown, the total count of dialog state stays the same.
  histogram_tester.ExpectTotalCount("WebApp.UrlHandling.DialogState", 1);
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogAccepted_RememberWebAppLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  base::HistogramTester histogram_tester;
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Get matches before dialog launch.
  auto url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(GURL(kStartUrl));

  // Select and remember the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION, 1);
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kAppAcceptedAndRememberChoice,
      1);

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());

  // Get matches after dialog is closed.
  auto new_url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(GURL(kStartUrl));
  ASSERT_NE(url_handler_matches, new_url_handler_matches);

  // Close the app window and start with the same URL again. App should be
  // launched directly.
  CloseBrowserSynchronously(app_browser);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  ui_test_utils::WaitForBrowserToOpen();
  // Verify app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Dialog wasn't shown, the total count of dialog state stays the same.
  histogram_tester.ExpectTotalCount("WebApp.UrlHandling.DialogState", 1);
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogAccepted_WebAppLaunch_InScopeUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 1);
  // kStartUrl is in app scope.
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       DialogAccepted_WebAppLaunch_DifferentOriginUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 1);

  // URL is not in app scope but matches url_handlers of installed app.
  GURL target_url = GURL("https://example.com/abc/def");
  ASSERT_TRUE(target_url.is_valid());
  SendAppleEventToOpenUrlToAppController(target_url);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Out-of-scope URL launch should open new app window and navigate to the
  // out-of-scope URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(target_url, web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupWebAppUrlHandlingBrowserTest,
    MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  // Create profiles and install URL Handling apps.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id_1 = web_app::test::InstallWebAppWithUrlHandlers(
      profile1, GURL(kStartUrl), kAppName, {url_handler});
  web_app::AppId app_id_2 = web_app::test::InstallWebAppWithUrlHandlers(
      profile2, GURL(kStartUrl), kAppName, {url_handler});

  // Test that we should be able to select the 3rd option.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 2);
  // kStartUrl is in app scope for both apps.
  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // There should be one app window. No deterministic ordering of apps, so find
  // which profile app is launched.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1) +
                    chrome::GetBrowserCount(profile2));
  Profile* app_profile =
      (chrome::GetBrowserCount(profile1) == 1) ? profile1 : profile2;
  Browser* app_browser = chrome::FindBrowserWithProfile(app_profile);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(
      web_app::AppBrowserController::IsForWebApp(app_browser, app_id_1) ||
      web_app::AppBrowserController::IsForWebApp(app_browser, app_id_2));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest,
                       CheckHistogramsFired) {
  base::HistogramTester histogram_tester;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  SendAppleEventToOpenUrlToAppController(GURL(kStartUrl));

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  histogram_tester.ExpectTotalCount(
      "WebApp.UrlHandling.GetValidProfilesAtStartUp", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.UrlHandling.LoadWebAppRegistrarsAtStartUp", 1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::kClosed, 1);
}

IN_PROC_BROWSER_TEST_F(StartupWebAppUrlHandlingBrowserTest, UrlNotCaptured) {
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // This URL is not in scope of installed app and does not match url_handlers.
  SendAppleEventToOpenUrlToAppController(
      GURL("https://en.example.com/abc/def"));

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  // Check the link of the new tab that was opened.
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(1);
  EXPECT_EQ(GURL("https://en.example.com/abc/def"),
            web_contents->GetVisibleURL());
}

}  // namespace

//--------------------------AppControllerHandoffBrowserTest---------------------

static GURL g_handoff_url;

@interface AppController (BrowserTest)
- (void)new_passURLToHandoffManager:(const GURL&)handoffURL;
@end

@implementation AppController (BrowserTest)
- (void)new_passURLToHandoffManager:(const GURL&)handoffURL {
  g_handoff_url = handoffURL;
}
@end

namespace {

class AppControllerHandoffBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerHandoffBrowserTest() {}

  // Exchanges the implementations of the two selectors on the class
  // AppController.
  void ExchangeSelectors(SEL originalMethod, SEL newMethod) {
    Class appControllerClass = NSClassFromString(@"AppController");

    ASSERT_TRUE(appControllerClass != nil);

    Method original =
        class_getInstanceMethod(appControllerClass, originalMethod);
    Method destination = class_getInstanceMethod(appControllerClass, newMethod);

    ASSERT_TRUE(original != NULL);
    ASSERT_TRUE(destination != NULL);

    method_exchangeImplementations(original, destination);
  }

  // Swizzle Handoff related implementations.
  void SetUpInProcessBrowserTestFixture() override {
    // This swizzle intercepts the URL that would be sent to the Handoff
    // Manager, and instead puts it into a variable accessible to this test.
    SEL originalMethod = @selector(passURLToHandoffManager:);
    SEL newMethod = @selector(new_passURLToHandoffManager:);
    ExchangeSelectors(originalMethod, newMethod);
  }

  // Closes the tab, and waits for the close to finish.
  void CloseTab(Browser* browser, int index) {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        browser->tab_strip_model()->GetWebContentsAt(index));
    browser->tab_strip_model()->CloseWebContentsAt(
        index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }
};

// Tests that as a user switches between tabs, navigates within a tab, and
// switches between browser windows, the correct URL is being passed to the
// Handoff.
IN_PROC_BROWSER_TEST_F(AppControllerHandoffBrowserTest, TestHandoffURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(g_handoff_url, GURL(url::kAboutBlankURL));

  // Test that navigating to a URL updates the handoff URL.
  GURL test_url1 = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), test_url1);
  EXPECT_EQ(g_handoff_url, test_url1);

  // Test that opening a new tab updates the handoff URL.
  GURL test_url2 = embedded_test_server()->GetURL("/title2.html");
  NavigateParams params(browser(), test_url2, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(g_handoff_url, test_url2);

  // Test that switching tabs updates the handoff URL.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(g_handoff_url, test_url1);

  // Test that closing the current tab updates the handoff URL.
  CloseTab(browser(), 0);
  EXPECT_EQ(g_handoff_url, test_url2);

  // Test that opening a new browser window updates the handoff URL.
  GURL test_url3 = embedded_test_server()->GetURL("/title3.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(test_url3), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(g_handoff_url, test_url3);

  // Check that there are exactly 2 browsers.
  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the second browser window (which only has 1 tab left).
  Browser* browser2 = active_browser_list->get(1);
  CloseBrowserSynchronously(browser2);
  EXPECT_EQ(g_handoff_url, test_url2);

  // The URLs of incognito windows should not be passed to Handoff.
  GURL test_url4 = embedded_test_server()->GetURL("/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(test_url4), WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(g_handoff_url, GURL());

  // Open a new tab in the incognito window.
  EXPECT_EQ(2u, active_browser_list->size());
  Browser* browser3 = active_browser_list->get(1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser3, test_url4, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  EXPECT_EQ(g_handoff_url, GURL());

  // Navigate the current tab in the incognito window.
  ui_test_utils::NavigateToURL(browser3, test_url1);
  EXPECT_EQ(g_handoff_url, GURL());

  // Activate the original browser window.
  Browser* browser1 = active_browser_list->get(0);
  browser1->window()->Show();
  EXPECT_EQ(g_handoff_url, test_url2);
}

}  // namespace
