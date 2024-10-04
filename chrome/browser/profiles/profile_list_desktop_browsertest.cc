// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

class ProfileListDesktopBrowserTest : public InProcessBrowserTest {
 public:
  ProfileListDesktopBrowserTest() {}

  ProfileListDesktopBrowserTest(const ProfileListDesktopBrowserTest&) = delete;
  ProfileListDesktopBrowserTest& operator=(
      const ProfileListDesktopBrowserTest&) = delete;

  std::unique_ptr<AvatarMenu> CreateAvatarMenu(
      ProfileAttributesStorage* storage) {
    return std::unique_ptr<AvatarMenu>(
        new AvatarMenu(storage, NULL, browser()));
  }

 private:
  std::unique_ptr<AvatarMenu> avatar_menu_;
};

#if BUILDFLAG(IS_CHROMEOS)
// This test doesn't make sense for Chrome OS since it has a different
// multi-profiles menu in the system tray instead.
#define MAYBE_SwitchToProfile DISABLED_SwitchToProfile
#else
#define MAYBE_SwitchToProfile SwitchToProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfileListDesktopBrowserTest, MAYBE_SwitchToProfile) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  base::FilePath path_profile1 = browser()->profile()->GetPath();

  base::FilePath path_profile2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  // Create an additional profile.
  profiles::testing::CreateProfileSync(profile_manager, path_profile2);

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  std::unique_ptr<AvatarMenu> menu = CreateAvatarMenu(&storage);
  menu->RebuildMenu();
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the first profile.
  menu->SwitchToProfile(
      menu->GetIndexOfItemWithProfilePathForTesting(path_profile1), false);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile. This is synchronous
  // on some platforms and asynchronous on others, so this code has to
  // handle both.
  menu->SwitchToProfile(
      menu->GetIndexOfItemWithProfilePathForTesting(path_profile2), false);
  if (browser_list->size() == 1) {
    ui_test_utils::WaitForBrowserToOpen();
  }
  EXPECT_EQ(2u, browser_list->size());

  // Switch to the first profile without opening a new window.
  menu->SwitchToProfile(
      menu->GetIndexOfItemWithProfilePathForTesting(path_profile1), false);
  EXPECT_EQ(2u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}
