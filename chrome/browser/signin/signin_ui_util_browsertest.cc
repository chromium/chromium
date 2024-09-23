// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_BROWSERTEST_CC_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_BROWSERTEST_CC_

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error This file only contains DICE browser tests for now.
#endif

namespace signin_ui_util {

class DiceSigninUiUtilBrowserTest : public InProcessBrowserTest {
 public:
  DiceSigninUiUtilBrowserTest() = default;
  ~DiceSigninUiUtilBrowserTest() override = default;

  Profile* CreateProfile() {
    Profile* new_profile = nullptr;
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"test_profile", /*icon_index=*/0, /*is_hidden=*/false,
        base::BindLambdaForTesting([&new_profile, &run_loop](Profile* profile) {
          ASSERT_TRUE(profile);
          new_profile = profile;
          run_loop.Quit();
        }));
    run_loop.Run();
    return new_profile;
  }

 private:
};

// Tests that `ShowExtensionSigninPrompt()` doesn't crash when it cannot create
// a new browser. Regression test for https://crbug.com/1273370.
IN_PROC_BROWSER_TEST_F(DiceSigninUiUtilBrowserTest,
                       ShowExtensionSigninPrompt_NoBrowser) {
  Profile* new_profile = CreateProfile();

  // New profile should not have any browser windows.
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));

  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  // `ShowExtensionSigninPrompt()` creates a new browser.
  Browser* browser = chrome::FindBrowserWithProfile(new_profile);
  ASSERT_TRUE(browser);
  EXPECT_EQ(1, browser->tab_strip_model()->count());

  // Profile deletion closes the browser.
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          new_profile->GetPath(), base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  ui_test_utils::WaitForBrowserToClose(browser);
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));

  // `ShowExtensionSigninPrompt()` does nothing for deleted profile.
  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));
}

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_BROWSERTEST_CC_
