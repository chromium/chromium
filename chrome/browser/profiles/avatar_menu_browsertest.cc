// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <memory>
#include <optional>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class AvatarMenuBrowserTest : public InProcessBrowserTest {
 public:
  AvatarMenuBrowserTest() {
    // Needed to avoid showing the browser window in a multi-profile use-case.
    set_open_about_blank_on_browser_launch(false);
  }
  ~AvatarMenuBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    menu_ = std::make_unique<AvatarMenu>(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
        nullptr, nullptr);
    menu_->RebuildMenu();
  }

  AvatarMenu* menu() { return menu_.get(); }

 private:
  std::unique_ptr<AvatarMenu> menu_;
};

IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, AddNewProfile) {
  EXPECT_TRUE(menu()->ShouldShowAddNewProfileLink());
  menu()->AddNewProfile();
  profiles::testing::WaitForPickerLoadStop(
      GURL("chrome://profile-picker/new-profile"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();

  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  EXPECT_FALSE(menu()->ShouldShowAddNewProfileLink());
}

IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, EditProfile) {
  std::optional<size_t> active_profile_index = menu()->GetActiveProfileIndex();
  ASSERT_TRUE(active_profile_index.has_value());
  ASSERT_EQ(menu()->GetItemAt(*active_profile_index).profile_path,
            browser()->profile()->GetPath());
  EXPECT_TRUE(menu()->ShouldShowEditProfileLink());
  menu()->EditProfile(*active_profile_index);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetVisibleURL(),
            chrome::GetSettingsUrl(chrome::kManageProfileSubPage));
}

// Click on "Edit" will open a new browser if none exists for a profile.
IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, EditProfile_NoBrowser) {
  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  Profile* profile = browser()->profile();
  BrowserList::CloseAllBrowsersWithProfile(profile);
  ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);

  std::optional<size_t> active_profile_index = menu()->GetActiveProfileIndex();
  ASSERT_TRUE(active_profile_index.has_value());
  ASSERT_EQ(menu()->GetItemAt(*active_profile_index).profile_path,
            profile->GetPath());
  EXPECT_TRUE(menu()->ShouldShowEditProfileLink());
  menu()->EditProfile(*active_profile_index);

  // A new browser is opened.
  EXPECT_EQ(chrome::GetBrowserCount(profile), 1U);
  Browser* new_browser = chrome::FindBrowserWithProfile(profile);
  ASSERT_TRUE(new_browser);
  content::WebContents* web_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetVisibleURL(),
            chrome::GetSettingsUrl(chrome::kManageProfileSubPage));
}

// "Edit" does not unlock the profile (regression test for
// https://crbug.com/1324958).
IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, EditProfile_SigninRequired) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  BrowserList::CloseAllBrowsersWithProfile(profile);
  ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);

  std::optional<size_t> active_profile_index = menu()->GetActiveProfileIndex();
  ASSERT_TRUE(active_profile_index.has_value());
  ASSERT_EQ(menu()->GetItemAt(*active_profile_index).profile_path,
            profile->GetPath());
  EXPECT_FALSE(menu()->ShouldShowEditProfileLink());
  menu()->EditProfile(*active_profile_index);

  // Browser shouldn't be opened since `profile` is locked.
  EXPECT_EQ(chrome::GetBrowserCount(profile), 0U);

  // The browser test doesn't shut down correctly if `keep_alive` is released
  // while there are no browser windows. Create browser to work around this
  // problem.
  entry->LockForceSigninProfile(false);
  CreateBrowser(profile);
}

// Sets up multiple profiles so that the profile picker is shown on next
// startup.
IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, PRE_EditProfile_NotLoaded) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& secondary_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  ui_test_utils::BrowserChangeObserver secondary_profile_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::NewEmptyWindow(&secondary_profile);
  // Wait for secondary profile browser window open and becomes the last active
  // one.
  ui_test_utils::WaitForBrowserSetLastActive(
      secondary_profile_browser_observer.Wait());

  // Close all browsers to avoid restoring profiles on the next startup.
  CloseAllBrowsers();
}

// "Edit" isn't enabled if no profiles are loaded.
IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, EditProfile_NotLoaded) {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 0U);
  EXPECT_FALSE(menu()->ShouldShowEditProfileLink());
  EXPECT_FALSE(menu()->GetActiveProfileIndex().has_value());
}

// Regression test for https://crbug.com/1382509
IN_PROC_BROWSER_TEST_F(AvatarMenuBrowserTest, Guest) {
  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 0U);

  profiles::SwitchToGuestProfile();
  Browser* guest_browser = ui_test_utils::WaitForBrowserToOpen();

  // ProfileManager will switch active profile upon observing
  // BrowserList::OnBrowserSetLastActive().
  bool wait_for_set_last_active_observed =
      !ProfileManager::GetLastUsedProfileIfLoaded()->IsGuestSession();
  ui_test_utils::WaitForBrowserSetLastActive(guest_browser,
                                             wait_for_set_last_active_observed);

  ASSERT_TRUE(guest_browser);
  ASSERT_TRUE(guest_browser->profile()->IsGuestSession());
  // This should not crash.
  EXPECT_FALSE(menu()->ShouldShowEditProfileLink());
}
