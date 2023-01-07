// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/scoped_objc_class_swizzler.h"
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

id* TargetForAction() {
  static id targetForAction;
  return &targetForAction;
}

}  // namespace

@interface FakeBrowserWindow : NSWindow
@end

@implementation FakeBrowserWindow
@end

// A class providing alternative implementations of various methods.
@interface AppControllerKeyEquivalentTestHelper : NSObject
- (id)targetForAction:(SEL)selector;
- (BOOL)windowHasBrowserTabs:(NSWindow*)window;
@end

@implementation AppControllerKeyEquivalentTestHelper

- (id)targetForAction:(SEL)selector {
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
  raw_ptr<TestingProfile> profile_;
};

class AppControllerKeyEquivalentTest : public PlatformTest {
 protected:
  AppControllerKeyEquivalentTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    nsAppTargetForActionSwizzler_ =
        std::make_unique<base::mac::ScopedObjCClassSwizzler>(
            [NSApp class], [AppControllerKeyEquivalentTestHelper class],
            @selector(targetForAction:));
    appControllerSwizzler_ =
        std::make_unique<base::mac::ScopedObjCClassSwizzler>(
            [AppController class], [AppControllerKeyEquivalentTestHelper class],
            @selector(windowHasBrowserTabs:));

    appController_.reset([[AppController alloc] init]);

    closeWindowMenuItem_.reset([[NSMenuItem alloc] initWithTitle:@""
                                                          action:0
                                                   keyEquivalent:@""]);
    [appController_ setCloseWindowMenuItemForTesting:closeWindowMenuItem_];

    closeTabMenuItem_.reset([[NSMenuItem alloc] initWithTitle:@""
                                                       action:0
                                                keyEquivalent:@""]);
    [appController_ setCloseTabMenuItemForTesting:closeTabMenuItem_];
  }

  void CheckMenuItemsMatchBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [appController_ updateMenuItemKeyEquivalents];

    EXPECT_TRUE([[closeWindowMenuItem_ keyEquivalent] isEqualToString:@"W"]);
    EXPECT_EQ([closeWindowMenuItem_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
    EXPECT_TRUE([[closeTabMenuItem_ keyEquivalent] isEqualToString:@"w"]);
    EXPECT_EQ([closeTabMenuItem_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
  }

  void CheckMenuItemsMatchNonBrowserWindow() {
    ASSERT_EQ([NSApp targetForAction:@selector(performClose:)],
              *TargetForAction());

    [appController_ updateMenuItemKeyEquivalents];

    EXPECT_TRUE([[closeWindowMenuItem_ keyEquivalent] isEqualToString:@"w"]);
    EXPECT_EQ([closeWindowMenuItem_ keyEquivalentModifierMask],
              NSEventModifierFlagCommand);
    EXPECT_TRUE([[closeTabMenuItem_ keyEquivalent] isEqualToString:@""]);
    EXPECT_EQ([closeTabMenuItem_ keyEquivalentModifierMask], 0UL);
  }

  void TearDown() override {
    PlatformTest::TearDown();

    [appController_ setCloseWindowMenuItemForTesting:nil];
    [appController_ setCloseTabMenuItemForTesting:nil];
    *TargetForAction() = nil;
  }

 private:
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler>
      nsAppTargetForActionSwizzler_;
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> appControllerSwizzler_;
  base::scoped_nsobject<AppController> appController_;
  base::scoped_nsobject<NSMenuItem> closeWindowMenuItem_;
  base::scoped_nsobject<NSMenuItem> closeTabMenuItem_;
};

TEST_F(AppControllerTest, DockMenuProfileNotLoaded) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  // Incognito item is hidden when the profile is not loaded.
  EXPECT_EQ(nil, [ac lastProfileIfLoaded]);
  EXPECT_EQ(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);
}

TEST_F(AppControllerTest, DockMenu) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  NSMenuItem* item;

  EXPECT_TRUE(menu);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_WINDOW]);

  // Incognito item is shown when the profile is loaded.
  EXPECT_EQ(profile_, [ac lastProfileIfLoaded]);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);

  for (item in [menu itemArray]) {
    EXPECT_EQ(ac.get(), [item target]);
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

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);

  // Delete the active profile.
  profile_manager_.profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          dest_path1, base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, [ac lastProfileIfLoaded]->GetPath());
}

// Tests key equivalents for Close Window when target is a child window (like a
// bubble).
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForBubbleWindow) {
  // Set up the "bubble" and main window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSWindow> childWindow([[NSWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  base::scoped_nsobject<NSWindow> browserWindow([[FakeBrowserWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);

  [browserWindow addChildWindow:childWindow ordered:NSWindowAbove];

  *TargetForAction() = childWindow;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is an NSPopOver.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForPopover) {
  // Set up the popover and main window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSPopover> popover([[NSPopover alloc] init]);
  base::scoped_nsobject<NSWindow> popoverWindow([[NSWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  [popover
      setContentViewController:[[[NSViewController alloc] init] autorelease]];
  [[popover contentViewController] setView:[popoverWindow contentView]];
  base::scoped_nsobject<NSWindow> browserWindow([[FakeBrowserWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  [browserWindow addChildWindow:popoverWindow ordered:NSWindowAbove];

  *TargetForAction() = popover;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is a browser window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForBrowserWindow) {
  // Set up the browser window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSWindow> browserWindow([[FakeBrowserWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);

  *TargetForAction() = browserWindow;

  CheckMenuItemsMatchBrowserWindow();
}

// Tests key equivalents for Close Window when target is not a browser window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForNonBrowserWindow) {
  // Set up the window.
  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSWindow> mainWindow([[NSWindow alloc]
      initWithContentRect:kContentRect
                styleMask:NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:YES]);

  *TargetForAction() = mainWindow;

  CheckMenuItemsMatchNonBrowserWindow();
}

// Tests key equivalents for Close Window when target is not a window.
TEST_F(AppControllerKeyEquivalentTest, UpdateMenuItemsForNonWindow) {
  base::scoped_nsobject<NSObject> nonWindowObject([[NSObject alloc] init]);
  *TargetForAction() = nonWindowObject;

  CheckMenuItemsMatchNonBrowserWindow();
}

class AppControllerSafeProfileTest : public AppControllerTest {
 protected:
  AppControllerSafeProfileTest() = default;
  ~AppControllerSafeProfileTest() override = default;

  void TearDown() override { [NSApp setDelegate:nil]; }
};

// Tests that RunInLastProfileSafely() works with an already-loaded
// profile.
TEST_F(AppControllerSafeProfileTest, LastProfileLoaded) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_->GetPath().BaseName().MaybeAsASCII());

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  [NSApp setDelegate:ac];
  ASSERT_EQ(profile_, [ac lastProfileIfLoaded]);

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

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  [NSApp setDelegate:ac];
  ASSERT_EQ(nil, [ac lastProfileIfLoaded]);

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

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  [NSApp setDelegate:ac];
  ASSERT_EQ(profile_, [ac lastProfileIfLoaded]);

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

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  [NSApp setDelegate:ac];
  ASSERT_EQ(profile_, [ac lastProfileIfLoaded]);

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
