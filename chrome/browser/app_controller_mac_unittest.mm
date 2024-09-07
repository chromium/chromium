// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

id __weak* TargetForAction() {
  static id __weak targetForAction;
  return &targetForAction;
}

}  // namespace

@interface FakeBrowserWindow : NSWindow
@end

@implementation FakeBrowserWindow
@end

// A class providing alternative implementations of various methods.
@interface AppControllerKeyEquivalentTestHelper : NSObject
- (id __weak)targetForAction:(SEL)selector;
- (BOOL)windowHasBrowserTabs:(NSWindow*)window;
@end

@implementation AppControllerKeyEquivalentTestHelper

- (id __weak)targetForAction:(SEL)selector {
  return *TargetForAction();
}

- (BOOL)windowHasBrowserTabs:(NSWindow*)window {
  return [window isKindOfClass:[FakeBrowserWindow class]];
}

@end

class AppControllerTest : public PlatformTest {
 protected:
  AppControllerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("New Profile 1");
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
};

class AppControllerKeyEquivalentTest : public PlatformTest {
 protected:
  AppControllerKeyEquivalentTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();

    _nsapp_target_for_action_swizzler =
        std::make_unique<base::apple::ScopedObjCClassSwizzler>(
            [NSApp class], [AppControllerKeyEquivalentTestHelper class],
            @selector(targetForAction:));
    _app_controller_swizzler =
        std::make_unique<base::apple::ScopedObjCClassSwizzler>(
            [AppController class], [AppControllerKeyEquivalentTestHelper class],
            @selector(windowHasBrowserTabs:));

    _app_controller = AppController.sharedController;

    _cmdw_menu_item =
        [[NSMenuItem alloc] initWithTitle:@""
                                   action:@selector(commandDispatch:)
                            keyEquivalent:@"w"];
    _cmdw_menu_item.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    _cmdw_menu_item.tag = IDC_CLOSE_TAB;
    [_app_controller setCmdWMenuItemForTesting:_cmdw_menu_item];

    _shift_cmdw_menu_item =
        [[NSMenuItem alloc] initWithTitle:@""
                                   action:@selector(performClose:)
                            keyEquivalent:@"W"];
    _shift_cmdw_menu_item.keyEquivalentModifierMask =
        NSEventModifierFlagCommand;
    _shift_cmdw_menu_item.tag = IDC_CLOSE_WINDOW;
    [_app_controller setShiftCmdWMenuItemForTesting:_shift_cmdw_menu_item];
  }

  void CheckMenuItemsMatchBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [_app_controller updateMenuItemKeyEquivalents];

    EXPECT_FALSE(_shift_cmdw_menu_item.hidden);
    EXPECT_EQ(_shift_cmdw_menu_item.tag, IDC_CLOSE_WINDOW);
    EXPECT_EQ(_shift_cmdw_menu_item.action, @selector(performClose:));
    EXPECT_TRUE([_shift_cmdw_menu_item.title
        isEqualToString:l10n_util::GetNSStringWithFixup(IDS_CLOSE_WINDOW_MAC)]);

    EXPECT_FALSE(_cmdw_menu_item.hidden);
    EXPECT_EQ(_cmdw_menu_item.tag, IDC_CLOSE_TAB);
    EXPECT_EQ(_cmdw_menu_item.action, @selector(commandDispatch:));
    EXPECT_TRUE([_cmdw_menu_item.title
        isEqualToString:l10n_util::GetNSStringWithFixup(IDS_CLOSE_TAB_MAC)]);
  }

  void CheckMenuItemsMatchNonBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [_app_controller updateMenuItemKeyEquivalents];

    EXPECT_TRUE(_shift_cmdw_menu_item.hidden);

    EXPECT_FALSE(_cmdw_menu_item.hidden);
    EXPECT_EQ(_cmdw_menu_item.tag, IDC_CLOSE_WINDOW);
    EXPECT_EQ(_cmdw_menu_item.action, @selector(performClose:));
    EXPECT_TRUE([_cmdw_menu_item.title
        isEqualToString:l10n_util::GetNSStringWithFixup(IDS_CLOSE_WINDOW_MAC)]);
  }

  // Check that we don't perform any shortcut switching when there's a custom
  // shortcut assigned to File->Close Window.
  void VerifyMenuItemsForCustomCloseWindowShortcut() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    NSMenuItem* close_tab_menu_item = _cmdw_menu_item;
    NSMenuItem* close_window_menu_item = _shift_cmdw_menu_item;

    // Assign a custom shortcut to Close Window.
    close_window_menu_item.keyEquivalent = @"w";
    close_window_menu_item.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagControl;

    [_app_controller updateMenuItemKeyEquivalents];

    // Both menu items should be undisturbed from their original states.
    EXPECT_FALSE(close_tab_menu_item.hidden);
    EXPECT_EQ(close_tab_menu_item.tag, IDC_CLOSE_TAB);
    EXPECT_EQ(close_tab_menu_item.action, @selector(commandDispatch:));
    EXPECT_TRUE([close_tab_menu_item.keyEquivalent isEqualToString:@"w"]);
    EXPECT_EQ(close_tab_menu_item.keyEquivalentModifierMask,
              NSEventModifierFlagCommand);

    EXPECT_FALSE(close_window_menu_item.hidden);
    EXPECT_EQ(close_window_menu_item.tag, IDC_CLOSE_WINDOW);
    EXPECT_EQ(close_window_menu_item.action, @selector(performClose:));
    EXPECT_TRUE([close_window_menu_item.keyEquivalent isEqualToString:@"w"]);
    EXPECT_EQ(close_window_menu_item.keyEquivalentModifierMask,
              NSEventModifierFlagCommand | NSEventModifierFlagControl);
  }

  // Check that we don't perform any shortcut switching when there's a custom
  // shortcut assigned to File->Close Tab.
  void VerifyMenuItemsForCustomCloseTabShortcut() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    NSMenuItem* close_tab_menu_item = _cmdw_menu_item;
    NSMenuItem* close_window_menu_item = _shift_cmdw_menu_item;

    // Assign a custom shortcut to Close Tab.
    close_tab_menu_item.keyEquivalent = @"w";
    close_tab_menu_item.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagControl;

    [_app_controller updateMenuItemKeyEquivalents];

    // Both menu items should be undisturbed from their original states.
    EXPECT_FALSE(close_tab_menu_item.hidden);
    EXPECT_EQ(close_tab_menu_item.tag, IDC_CLOSE_TAB);
    EXPECT_EQ(close_tab_menu_item.action, @selector(commandDispatch:));
    EXPECT_TRUE([close_tab_menu_item.keyEquivalent isEqualToString:@"w"]);
    EXPECT_EQ(close_tab_menu_item.keyEquivalentModifierMask,
              NSEventModifierFlagCommand | NSEventModifierFlagControl);

    EXPECT_FALSE(close_window_menu_item.hidden);
    EXPECT_EQ(close_window_menu_item.tag, IDC_CLOSE_WINDOW);
    EXPECT_EQ(close_window_menu_item.action, @selector(performClose:));
    EXPECT_TRUE([close_window_menu_item.keyEquivalent isEqualToString:@"W"]);
    EXPECT_EQ(close_window_menu_item.keyEquivalentModifierMask,
              NSEventModifierFlagCommand);
  }

  void TearDown() override {
    PlatformTest::TearDown();

    [_app_controller setCmdWMenuItemForTesting:nil];
    [_app_controller setShiftCmdWMenuItemForTesting:nil];
    *TargetForAction() = nil;
  }

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      _nsapp_target_for_action_swizzler;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      _app_controller_swizzler;
  AppController* __strong _app_controller;
  NSMenuItem* __strong _cmdw_menu_item;
  NSMenuItem* __strong _shift_cmdw_menu_item;
};

TEST_F(AppControllerTest, DockMenuProfileNotLoaded) {
  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  // Incognito item is hidden when the profile is not loaded.
  EXPECT_EQ(nil, [app_controller lastProfileIfLoaded]);
  EXPECT_EQ(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);
}

TEST_F(AppControllerTest, DockMenu) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;
  NSMenu* menu = [app_controller applicationDockMenu:NSApp];
  NSMenuItem* item;

  EXPECT_TRUE(menu);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_WINDOW]);

  // Incognito item is shown when the profile is loaded.
  EXPECT_EQ(profile_, [app_controller lastProfileIfLoaded]);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);

  for (item in [menu itemArray]) {
    EXPECT_EQ(app_controller, [item target]);
    EXPECT_EQ(@selector(commandFromDock:), [item action]);
  }
}

TEST_F(AppControllerTest, LastProfileIfLoaded) {
  // Create a second profile.
  base::FilePath dest_path1 = profile_->GetPath();
  base::FilePath dest_path2 =
      profile_manager_.CreateTestingProfile("New Profile 2")->GetPath();
  ASSERT_EQ(2U, profile_manager_.profile_manager()->GetNumberOfProfiles());
  ASSERT_EQ(2U, profile_manager_.profile_manager()->GetLoadedProfiles().size());

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;

  // Delete the active profile.
  profile_manager_.profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          dest_path1, base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, app_controller.lastProfileIfLoaded->GetPath());
}

// Tests key equivalents for Close Window when target is a child window (like a
// bubble).
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForBubbleWindow) {
  // Set up the "bubble" and main window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* child_window =
      [[NSWindow alloc] initWithContentRect:kContentRect
                                  styleMask:NSWindowStyleMaskClosable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  child_window.releasedWhenClosed = NO;
  NSWindow* browser_window =
      [[FakeBrowserWindow alloc] initWithContentRect:kContentRect
                                           styleMask:NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:YES];
  browser_window.releasedWhenClosed = NO;

  [browser_window addChildWindow:child_window ordered:NSWindowAbove];

  *TargetForAction() = child_window;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is an NSPopOver.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForPopover) {
  // Set up the popover and main window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSPopover* popover = [[NSPopover alloc] init];
  NSWindow* popover_window =
      [[NSWindow alloc] initWithContentRect:kContentRect
                                  styleMask:NSWindowStyleMaskClosable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  popover_window.releasedWhenClosed = NO;

  [popover setContentViewController:[[NSViewController alloc] init]];
  [[popover contentViewController] setView:[popover_window contentView]];

  NSWindow* browser_window =
      [[FakeBrowserWindow alloc] initWithContentRect:kContentRect
                                           styleMask:NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:YES];
  browser_window.releasedWhenClosed = NO;
  [browser_window addChildWindow:popover_window ordered:NSWindowAbove];

  *TargetForAction() = popover;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is a browser window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForBrowserWindow) {
  // Set up the browser window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* browser_window =
      [[FakeBrowserWindow alloc] initWithContentRect:kContentRect
                                           styleMask:NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:YES];

  *TargetForAction() = browser_window;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is a descendant of a
// browser window.
TEST_F(AppControllerKeyEquivalentTest,
       UpdateMenuItemsForBrowserWindowDescendant) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImmersiveFullscreen);

  // Set up the browser window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* browser_window =
      [[FakeBrowserWindow alloc] initWithContentRect:kContentRect
                                           styleMask:NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:YES];

  // Set up descendants.
  NSWindow* child_window = [[NSWindow alloc] init];
  [browser_window addChildWindow:child_window ordered:NSWindowAbove];
  NSWindow* child_child_window = [[NSWindow alloc] init];
  [child_window addChildWindow:child_child_window ordered:NSWindowAbove];

  *TargetForAction() = child_child_window;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is not a browser window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForNonBrowserWindow) {
  // Set up the window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* main_window =
      [[NSWindow alloc] initWithContentRect:kContentRect
                                  styleMask:NSWindowStyleMaskClosable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];

  *TargetForAction() = main_window;

  CheckMenuItemsMatchNonBrowserWindow();
}

// Tests key equivalents for Close Window when target is not a window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForNonWindow) {
  NSObject* non_window_object = [[NSObject alloc] init];
  *TargetForAction() = non_window_object;

  CheckMenuItemsMatchNonBrowserWindow();
}

// Tests key equivalents for Close Window and Close Tab when we shift from one
// browser window to no browser windows, and then back to one browser window.
TEST_F(AppControllerKeyEquivalentTest, MenuItemsUpdateWithWindowChanges) {
  // Set up the browser window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* browser_window =
      [[FakeBrowserWindow alloc] initWithContentRect:kContentRect
                                           styleMask:NSWindowStyleMaskClosable
                                             backing:NSBackingStoreBuffered
                                               defer:YES];

  *TargetForAction() = browser_window;

  CheckMenuItemsMatchBrowserWindow();

  // "Close" it.
  NSObject* non_window_object = [[NSObject alloc] init];
  *TargetForAction() = non_window_object;

  CheckMenuItemsMatchNonBrowserWindow();

  // "New" window.
  *TargetForAction() = browser_window;

  CheckMenuItemsMatchBrowserWindow();
}

TEST_F(AppControllerKeyEquivalentTest,
       DontChangeShortcutsWhenCustomCloseWindowShortcutAssigned) {
  // Set up the window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* main_window =
      [[NSWindow alloc] initWithContentRect:kContentRect
                                  styleMask:NSWindowStyleMaskClosable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];

  *TargetForAction() = main_window;

  VerifyMenuItemsForCustomCloseWindowShortcut();
}

TEST_F(AppControllerKeyEquivalentTest,
       DontChangeShortcutsWhenCustomCloseTabShortcutAssigned) {
  // Set up the window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  NSWindow* main_window =
      [[NSWindow alloc] initWithContentRect:kContentRect
                                  styleMask:NSWindowStyleMaskClosable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];

  *TargetForAction() = main_window;

  VerifyMenuItemsForCustomCloseTabShortcut();
}

class AppControllerSafeProfileTest : public AppControllerTest {
 protected:
  AppControllerSafeProfileTest() = default;
  ~AppControllerSafeProfileTest() override = default;
};

// Tests that RunInLastProfileSafely() works with an already-loaded
// profile.
TEST_F(AppControllerSafeProfileTest, LastProfileLoaded) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(profile_, app_controller.lastProfileIfLoaded);

  base::RunLoop run_loop;
  app_controller_mac::RunInLastProfileSafely(
      base::BindLambdaForTesting([&](Profile* profile) {
        EXPECT_EQ(profile, profile_.get());
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}

// Tests that RunInLastProfileSafely() re-loads the profile from disk if
// it's not currently in memory.
TEST_F(AppControllerSafeProfileTest, LastProfileNotLoaded) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, "New Profile 2");

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(nil, app_controller.lastProfileIfLoaded);

  base::RunLoop run_loop;
  app_controller_mac::RunInLastProfileSafely(
      base::BindLambdaForTesting([&](Profile* profile) {
        EXPECT_NE(profile, nullptr);
        EXPECT_NE(profile, profile_.get());
        EXPECT_EQ(profile->GetBaseName().MaybeAsASCII(), "New Profile 2");
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}

// Tests that RunInProfileInSafeProfileHelper::RunInProfile() works with an
// already-loaded profile.
TEST_F(AppControllerSafeProfileTest, SpecificProfileLoaded) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(profile_, app_controller.lastProfileIfLoaded);

  TestingProfile* profile2 =
      profile_manager_.CreateTestingProfile("New Profile 2");

  base::RunLoop run_loop;
  app_controller_mac::RunInProfileSafely(
      profile_manager_.profiles_dir().AppendASCII("New Profile 2"),
      base::BindLambdaForTesting([&](Profile* profile) {
        // This should run with the specific profile we asked for, rather than
        // the last-used profile.
        EXPECT_EQ(profile, profile2);
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}

// Tests that RunInProfileSafely() re-loads the profile from
// disk if it's not currently in memory.
TEST_F(AppControllerSafeProfileTest, SpecificProfileNotLoaded) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(profile_, app_controller.lastProfileIfLoaded);

  // Add a profile in the cache (simulate another profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"New Profile 2";
  profile_storage->AddProfile(std::move(params));

  base::RunLoop run_loop;
  app_controller_mac::RunInProfileSafely(
      profile_path, base::BindLambdaForTesting([&](Profile* profile) {
        // This should run with the specific profile we asked for, rather than
        // the last-used profile.
        EXPECT_NE(profile, nullptr);
        EXPECT_NE(profile, profile_.get());
        EXPECT_EQ(profile->GetPath(), profile_path);
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}

// Tests that RunInProfileSafely() returns nullptr if a profle doesn't exist.
TEST_F(AppControllerSafeProfileTest, SpecificProfileDoesNotExist) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  AppController* app_controller = AppController.sharedController;
  ASSERT_EQ(profile_, app_controller.lastProfileIfLoaded);

  base::RunLoop run_loop;
  app_controller_mac::RunInProfileSafely(
      profile_manager_.profiles_dir().AppendASCII("Non-existent Profile"),
      base::BindLambdaForTesting([&](Profile* profile) {
        EXPECT_EQ(profile, nullptr);
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}
