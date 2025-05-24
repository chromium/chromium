// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_os_info_override_win.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/gfx/image/image_unittest_util.h"

class ProfileShortcutManagerBrowserTest : public InProcessBrowserTest {
 public:
  ProfileShortcutManagerBrowserTest() = default;
  ~ProfileShortcutManagerBrowserTest() override = default;
  ProfileShortcutManagerBrowserTest(const ProfileShortcutManagerBrowserTest&) =
      delete;
  ProfileShortcutManagerBrowserTest& operator=(
      const ProfileShortcutManagerBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableProfileShortcutManager);
  }

  ProfileAttributesEntry* GetProfileAttributesEntry() {
    if (!browser() || !browser()->profile())
      return nullptr;
    return g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(browser()->profile()->GetPath());
  }

  std::string ReadProfileIcon() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string icon;
    EXPECT_TRUE(base::ReadFileToString(
        profiles::internal::GetProfileIconPath(browser()->profile()->GetPath()),
        &icon));
    EXPECT_FALSE(icon.empty());
    return icon;
  }
};

IN_PROC_BROWSER_TEST_F(ProfileShortcutManagerBrowserTest,
                       PRE_UpdateProfileIconOnAvatarLoaded) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(identity_manager);
  signin::MakePrimaryAccountAvailable(identity_manager, "foo@gmail.com",
                                      signin::ConsentLevel::kSync);
  // Standard size of a Gaia account picture.
  const int kGaiaPictureSize = 256;
  gfx::Image gaia_image(
      gfx::test::CreateImage(kGaiaPictureSize, kGaiaPictureSize));
  ProfileAttributesEntry* entry = GetProfileAttributesEntry();
  ASSERT_NE(nullptr, entry);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE_1", gaia_image);

  // We need at least two profiles because a profile icon won't be badged if the
  // number of profiles is one.
  base::FilePath path_profile2 =
      g_browser_process->profile_manager()->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(g_browser_process->profile_manager(),
                                       path_profile2);

  // This is for triggering a profile icon update on the next run. 1 is just a
  // small enough number for kCurrentProfileIconVersion.
  browser()->profile()->GetPrefs()->SetInteger(prefs::kProfileIconVersion, 1);

  // Ensure that any tasks started by profile creation are finished before we
  // advance to the main test. In particular, we want to finish all tasks that
  // might update the profile icon before the main test runs.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ProfileShortcutManagerBrowserTest,
                       UpdateProfileIconOnAvatarLoaded) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedPathOverride desktop_override(base::DIR_USER_DESKTOP);
  std::string badged_icon;
  EXPECT_NO_FATAL_FAILURE(badged_icon = ReadProfileIcon());

  ProfileAttributesEntry* entry = GetProfileAttributesEntry();
  ASSERT_NE(nullptr, entry);
  ASSERT_TRUE(entry->IsGAIAPictureLoaded());
  g_browser_process->profile_manager()
      ->profile_shortcut_manager()
      ->CreateOrUpdateProfileIcon(browser()->profile()->GetPath());
  content::RunAllTasksUntilIdle();

  std::string badged_icon_with_gaia_picture;
  EXPECT_NO_FATAL_FAILURE(badged_icon_with_gaia_picture = ReadProfileIcon());

  // badged_icon might have been created with a default avatar image and then
  // updated with GAIA picture.
  EXPECT_EQ(badged_icon, badged_icon_with_gaia_picture);
}

IN_PROC_BROWSER_TEST_F(ProfileShortcutManagerBrowserTest,
                       ProfileIconWin10VersionPrefTest) {
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin10Pro);
  base::ScopedAllowBlockingForTesting allow_blocking;
  g_browser_process->profile_manager()
      ->profile_shortcut_manager()
      ->CreateOrUpdateProfileIcon(browser()->profile()->GetPath());
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kProfileIconWin11Format));
}

IN_PROC_BROWSER_TEST_F(ProfileShortcutManagerBrowserTest,
                       ProfileIconWin11VersionPrefTest) {
  base::test::ScopedOSInfoOverride os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
  base::ScopedAllowBlockingForTesting allow_blocking;
  g_browser_process->profile_manager()
      ->profile_shortcut_manager()
      ->CreateOrUpdateProfileIcon(browser()->profile()->GetPath());
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kProfileIconWin11Format));
}

class ProfileIconUpgradeBrowserTest : public ProfileShortcutManagerBrowserTest {
 public:
  ProfileIconUpgradeBrowserTest() = default;
  ~ProfileIconUpgradeBrowserTest() override = default;
  ProfileIconUpgradeBrowserTest(const ProfileIconUpgradeBrowserTest&) = delete;
  ProfileIconUpgradeBrowserTest& operator=(
      const ProfileIconUpgradeBrowserTest&) = delete;

  void SetUp() override {
    os_override_ = std::make_unique<base::test::ScopedOSInfoOverride>(
        GetTestPreCount() > 0
            ? base::test::ScopedOSInfoOverride::Type::kWin10Pro
            : base::test::ScopedOSInfoOverride::Type::kWin11Pro);
    ProfileShortcutManagerBrowserTest::SetUp();
  }

 protected:
  // Initialized in SetUp() to Win10 for PRE_ test and Win11 for test.
  std::unique_ptr<base::test::ScopedOSInfoOverride> os_override_;
};

// The PRE test creates a profile icon in Win 10 format.
IN_PROC_BROWSER_TEST_F(ProfileIconUpgradeBrowserTest,
                       PRE_UpgradeWin10ToWin11Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Force icon to be Win10 format.
  g_browser_process->profile_manager()
      ->profile_shortcut_manager()
      ->CreateOrUpdateProfileIcon(browser()->profile()->GetPath());
  content::RunAllTasksUntilIdle();
  LOG(INFO) << "finished running all tasks";
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kProfileIconWin11Format));
}

// This test forces the OS version to Win 11, and checks that the profile
// icon was migrated to Win 11.
IN_PROC_BROWSER_TEST_F(ProfileIconUpgradeBrowserTest, UpgradeWin10ToWin11Test) {
  // Loading the profile should trigger a migration of the profile icon, and
  // a corresponding set of the icon format pref.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kProfileIconWin11Format));
}
