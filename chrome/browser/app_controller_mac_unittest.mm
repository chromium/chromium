// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/platform_test.h"

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

    nsapp_target_for_action_swizzler_ =
        std::make_unique<base::mac::ScopedObjCClassSwizzler>(
            [NSApp class], [AppControllerKeyEquivalentTestHelper class],
            @selector(targetForAction:));
    app_controller_swizzler_ =
        std::make_unique<base::mac::ScopedObjCClassSwizzler>(
            [AppController class], [AppControllerKeyEquivalentTestHelper class],
            @selector(windowHasBrowserTabs:));

    app_controller_ = AppController.sharedController;

    close_window_menu_item_ = [[NSMenuItem alloc] initWithTitle:@""
                                                         action:nullptr
                                                  keyEquivalent:@""];
    [app_controller_ setCloseWindowMenuItemForTesting:close_window_menu_item_];

    close_tab_menu_item_ = [[NSMenuItem alloc] initWithTitle:@""
                                                      action:nullptr
                                               keyEquivalent:@""];
    [app_controller_ setCloseTabMenuItemForTesting:close_tab_menu_item_];
  }

  void CheckMenuItemsMatchBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [app_controller_ updateMenuItemKeyEquivalents];

    EXPECT_TRUE([[close_window_menu_item_ keyEquivalent] isEqualToString:@"W"]);
    EXPECT_EQ([close_window_menu_item_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
    EXPECT_TRUE([[close_tab_menu_item_ keyEquivalent] isEqualToString:@"w"]);
    EXPECT_EQ([close_tab_menu_item_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
  }

  void CheckMenuItemsMatchNonBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [app_controller_ updateMenuItemKeyEquivalents];

    EXPECT_TRUE([[close_window_menu_item_ keyEquivalent] isEqualToString:@"w"]);
    EXPECT_EQ([close_window_menu_item_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
    EXPECT_TRUE([[close_tab_menu_item_ keyEquivalent] isEqualToString:@""]);
    EXPECT_EQ([close_tab_menu_item_ keyEquivalentModifierMask], 0UL);
  }

  void TearDown() override {
    PlatformTest::TearDown();

    [app_controller_ setCloseWindowMenuItemForTesting:nil];
    [app_controller_ setCloseTabMenuItemForTesting:nil];
    *TargetForAction() = nil;
  }

 private:
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler>
      nsapp_target_for_action_swizzler_;
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> app_controller_swizzler_;
  AppController* __strong app_controller_;
  NSMenuItem* __strong close_window_menu_item_;
  NSMenuItem* __strong close_tab_menu_item_;
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

  base::RunLoop run_loop;
  app_controller_mac::RunInProfileSafely(
      profile_manager_.profiles_dir().AppendASCII("New Profile 2"),
      base::BindLambdaForTesting([&](Profile* profile) {
        // This should run with the specific profile we asked for, rather than
        // the last-used profile.
        EXPECT_NE(profile, nullptr);
        EXPECT_NE(profile, profile_.get());
        EXPECT_EQ(profile->GetBaseName().MaybeAsASCII(), "New Profile 2");
        run_loop.Quit();
      }),
      app_controller_mac::kIgnoreOnFailure);
  run_loop.Run();
}
