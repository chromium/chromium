// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(Profile* profile,
                                Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

class ProfileListDesktopBrowserTest : public InProcessBrowserTest {
 public:
  ProfileListDesktopBrowserTest() {}

  std::unique_ptr<AvatarMenu> CreateAvatarMenu(
      ProfileAttributesStorage* storage) {
    return std::unique_ptr<AvatarMenu>(
        new AvatarMenu(storage, NULL, browser()));
  }

 private:
  std::unique_ptr<AvatarMenu> avatar_menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktopBrowserTest);
};

#if defined(OS_WIN) || (defined(OS_MACOSX) && defined(ADDRESS_SANITIZER))
// SignOut is flaky on Windows, crbug.com/357329,
// and Mac with ASAN, crbug.com/674497.
#define MAYBE_SignOut DISABLED_SignOut
#elif defined(OS_CHROMEOS)
// This test doesn't make sense for Chrome OS since it has a different
// multi-profiles menu in the system tray instead.
#define MAYBE_SignOut DISABLED_SignOut
#elif defined(OS_LINUX)
// Flaky on Linux debug builds with libc++ (https://crbug.com/734875)
#define MAYBE_SignOut DISABLED_SignOut
#else
#define MAYBE_SignOut SignOut
#endif
IN_PROC_BROWSER_TEST_F(ProfileListDesktopBrowserTest, MAYBE_SignOut) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser()->profile();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(current_profile->GetPath(),
                                                   &entry));

  std::unique_ptr<AvatarMenu> menu = CreateAvatarMenu(&storage);
  menu->RebuildMenu();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());

  content::WindowedNotificationObserver system_profile_created_observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  EXPECT_FALSE(entry->IsSigninRequired());
  profiles::LockProfile(current_profile);
  // Rely on test time-out for failure indication.
  ui_test_utils::WaitForBrowserToClose(browser());

  EXPECT_TRUE(entry->IsSigninRequired());
  EXPECT_EQ(0u, browser_list->size());

  // Signing out brings up the User Manager which we should close before exit.
  // But the User Manager is shown only when the system profile is created,
  // which happens asynchronously.
  system_profile_created_observer.Wait();
  UserManager::Hide();
}

#if defined(OS_CHROMEOS)
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

  // Create an additional profile.
  base::FilePath path_profile2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  profile_manager->CreateProfileAsync(path_profile2,
                                      base::Bind(&OnUnblockOnProfileCreation),
                                      base::string16(), std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  std::unique_ptr<AvatarMenu> menu = CreateAvatarMenu(&storage);
  menu->RebuildMenu();
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the first profile.
  menu->SwitchToProfile(menu->GetIndexOfItemWithProfilePath(path_profile1),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  menu->SwitchToProfile(menu->GetIndexOfItemWithProfilePath(path_profile2),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2u, browser_list->size());

  // Switch to the first profile without opening a new window.
  menu->SwitchToProfile(menu->GetIndexOfItemWithProfilePath(path_profile1),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2u, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}
