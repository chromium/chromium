// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using AppControllerInteractiveUITest = InProcessBrowserTest;

namespace {

IN_PROC_BROWSER_TEST_F(AppControllerInteractiveUITest,
                       OpenURLsDontActivateBrowserWindow) {
  // Tests that application:openURLs does not activate the browser window. The
  // LaunchServices will send activation events if necessary. Force activating
  // the browser window can cause undesirable behavior, for example, when the
  // URL was opened by another application with
  // NSWorkspaceLaunchWithoutActivation or
  // NSWorkspaceOpenConfiguration.activates set to NO, the browser window
  // becomes activated anyway.
  Profile* default_profile = browser()->profile();

  CloseBrowserSynchronously(browser());
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());

  AppController* ac = base::mac::ObjCCast<AppController>([NSApp delegate]);
  ASSERT_TRUE(ac);
  [ac application:NSApp
         openURLs:@[ [NSURL URLWithString:@"http://example.com"] ]];
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  Browser* new_browser = chrome::FindBrowserWithProfile(default_profile);
  ASSERT_TRUE(new_browser);
  EXPECT_FALSE(new_browser->window()->IsActive());

  [ac application:NSApp
         openURLs:@[ [NSURL URLWithString:@"http://example.com"] ]];
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_FALSE(new_browser->window()->IsActive());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());

  // Minimized browser windows should be treated as exceptions and have to be
  // activated since they won't receive activation events from the OS.
  new_browser->window()->Minimize();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(new_browser->window()->IsMinimized());
  EXPECT_FALSE(new_browser->window()->IsActive());

  [ac application:NSApp
         openURLs:@[ [NSURL URLWithString:@"http://example.com"] ]];

  EXPECT_TRUE(new_browser->window()->IsActive());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_browser->window()->IsMinimized());
}

}  // namespace
