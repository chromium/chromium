// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class ProfileShortcutManagerTest : public testing::Test {
 protected:
  ProfileShortcutManagerTest()
      : profile_attributes_storage_(nullptr),
        fake_user_desktop_(base::DIR_USER_DESKTOP),
        fake_system_desktop_(base::DIR_COMMON_DESKTOP) {
  }

  void SetUp() override {
    TestingBrowserProcess* browser_process =
        TestingBrowserProcess::GetGlobal();
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_attributes_storage_ =
        profile_manager_->profile_attributes_storage();
    profile_shortcut_manager_ =
        ProfileShortcutManager::Create(profile_manager_->profile_manager());
    profile_1_name_ = L"My profile";
    profile_1_path_ = CreateProfileDirectory(profile_1_name_);
    profile_2_name_ = L"My profile 2";
    profile_2_path_ = CreateProfileDirectory(profile_2_name_);
    profile_3_name_ = L"My profile 3";
    profile_3_path_ = CreateProfileDirectory(profile_3_name_);
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // Delete all profiles and ensure their shortcuts got removed.
    const size_t num_profiles =
        profile_attributes_storage_->GetNumberOfProfiles();
    for (size_t i = 0; i < num_profiles; ++i) {
      ProfileAttributesEntry* entry =
          profile_attributes_storage_->GetAllProfilesAttributes().front();
      const base::FilePath profile_path = entry->GetPath();
      base::string16 profile_name = entry->GetName();
      profile_attributes_storage_->RemoveProfile(profile_path);
      task_environment_.RunUntilIdle();
      ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name));
      // The icon file is not deleted until the profile directory is deleted.
      const base::FilePath icon_path =
          profiles::internal::GetProfileIconPath(profile_path);
      ASSERT_TRUE(base::PathExists(icon_path));
    }
  }

  base::FilePath CreateProfileDirectory(const base::string16& profile_name) {
    const base::FilePath profile_path =
        profile_manager_->profiles_dir().Append(profile_name);
    base::CreateDirectory(profile_path);
    return profile_path;
  }

  void SetupDefaultProfileShortcut(const base::Location& location) {
    ASSERT_EQ(0u, profile_attributes_storage_->GetNumberOfProfiles())
        << location.ToString();
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_))
        << location.ToString();
    profile_attributes_storage_->AddProfile(
        profile_1_path_, profile_1_name_, std::string(), base::string16(),
        false, 0, std::string(), EmptyAccountId());
    // Also create a non-badged shortcut for Chrome, which is conveniently done
    // by |CreateProfileShortcut()| since there is only one profile.
    profile_shortcut_manager_->CreateProfileShortcut(profile_1_path_);
    task_environment_.RunUntilIdle();
    // Verify that there's now an unbadged, unmamed shortcut for the single
    // profile.
    ValidateSingleProfileShortcut(location, profile_1_path_);
  }

  void SetupNonProfileShortcut(const base::Location& location) {
    ASSERT_EQ(0u, profile_attributes_storage_->GetNumberOfProfiles())
        << location.ToString();
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_))
        << location.ToString();
    profile_attributes_storage_->AddProfile(
        profile_1_path_, profile_1_name_, std::string(), base::string16(),
        false, 0, std::string(), EmptyAccountId());
    // Create a non profile shortcut for Chrome.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::FilePath chrome_exe;
    if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
      NOTREACHED();
      return;
    }

    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(chrome_exe, &properties);

    properties.set_app_id(
        shell_integration::win::GetChromiumModelIdForProfile(profile_1_path_));

    const base::FilePath shortcut_path(
        profiles::internal::GetShortcutFilenameForProfile(L""));

    const base::FilePath new_shortcut_name =
        shortcut_path.BaseName().RemoveExtension();
    properties.set_shortcut_name(new_shortcut_name.value());
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);
    task_environment_.RunUntilIdle();
    // Verify that there's now an unbadged, unmamed shortcut for the single
    // profile.
    ValidateNonProfileShortcut(location);
  }

  void SetupAndCreateTwoShortcuts(const base::Location& location) {
    SetupDefaultProfileShortcut(location);
    CreateProfileWithShortcut(location, profile_2_name_, profile_2_path_);
    ValidateProfileShortcut(location, profile_1_name_, profile_1_path_);
  }

  void SetupTwoProfilesAndNonProfileShortcut(const base::Location& location) {
    SetupNonProfileShortcut(location);
    CreateProfileWithShortcut(location, profile_2_name_, profile_2_path_);
  }

  // Returns the default shortcut path for this profile.
  base::FilePath GetDefaultShortcutPathForProfile(
      const base::string16& profile_name) {
    return GetUserShortcutsDirectory().Append(
        profiles::internal::GetShortcutFilenameForProfile(profile_name));
  }

  // Returns true if the shortcut for this profile exists.
  bool ProfileShortcutExistsAtDefaultPath(const base::string16& profile_name) {
    return base::PathExists(
        GetDefaultShortcutPathForProfile(profile_name));
  }

  // Posts a task to call base::win::ValidateShortcut on the COM thread.
  void PostValidateShortcut(
      const base::Location& location,
      const base::FilePath& shortcut_path,
      const base::win::ShortcutProperties& expected_properties) {
    base::CreateCOMSTATaskRunner({base::ThreadPool()})
        ->PostTask(location, base::Bind(&base::win::ValidateShortcut,
                                        shortcut_path, expected_properties));
    task_environment_.RunUntilIdle();
  }

  // Calls base::win::ValidateShortcut() with expected properties for the
  // shortcut at |shortcut_path| for the profile at |profile_path|.
  void ValidateProfileShortcutAtPath(const base::Location& location,
                                     const base::FilePath& shortcut_path,
                                     const base::FilePath& profile_path) {
    EXPECT_TRUE(base::PathExists(shortcut_path)) << location.ToString();

    // Ensure that the corresponding icon exists.
    const base::FilePath icon_path =
        profiles::internal::GetProfileIconPath(profile_path);
    EXPECT_TRUE(base::PathExists(icon_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_app_id(
        shell_integration::win::GetChromiumModelIdForProfile(profile_path));
    expected_properties.set_target(GetExePath());
    expected_properties.set_description(InstallUtil::GetAppDescription());
    expected_properties.set_dual_mode(false);
    expected_properties.set_arguments(
        profiles::internal::CreateProfileShortcutFlags(profile_path));
    expected_properties.set_icon(icon_path, 0);
    PostValidateShortcut(location, shortcut_path, expected_properties);
  }

  // Calls base::win::ValidateShortcut() with expected properties for
  // |profile_name|'s shortcut.
  void ValidateProfileShortcut(const base::Location& location,
                               const base::string16& profile_name,
                               const base::FilePath& profile_path) {
    ValidateProfileShortcutAtPath(
        location, GetDefaultShortcutPathForProfile(profile_name), profile_path);
  }

  void ValidateSingleProfileShortcutAtPath(
      const base::Location& location,
      const base::FilePath& profile_path,
      const base::FilePath& shortcut_path) {
    EXPECT_TRUE(base::PathExists(shortcut_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(GetExePath());
    expected_properties.set_arguments(
        profiles::internal::CreateProfileShortcutFlags(profile_path));
    expected_properties.set_icon(GetExePath(), 0);
    expected_properties.set_description(InstallUtil::GetAppDescription());
    expected_properties.set_dual_mode(false);
    PostValidateShortcut(location, shortcut_path, expected_properties);
  }

  void ValidateSingleProfileShortcut(const base::Location& location,
                                     const base::FilePath& profile_path) {
    const base::FilePath shortcut_path =
        GetDefaultShortcutPathForProfile(base::string16());
    ValidateSingleProfileShortcutAtPath(location, profile_path, shortcut_path);
  }

  void ValidateNonProfileShortcut(const base::Location& location) {
    const base::FilePath shortcut_path =
        GetDefaultShortcutPathForProfile(base::string16());
    EXPECT_TRUE(base::PathExists(shortcut_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(GetExePath());
    expected_properties.set_arguments(base::string16());
    expected_properties.set_icon(GetExePath(), 0);
    expected_properties.set_description(InstallUtil::GetAppDescription());
    expected_properties.set_dual_mode(false);
    PostValidateShortcut(location, shortcut_path, expected_properties);
  }

  void CreateProfileWithShortcut(const base::Location& location,
                                 const base::string16& profile_name,
                                 const base::FilePath& profile_path) {
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name))
        << location.ToString();
    profile_attributes_storage_->AddProfile(
        profile_path, profile_name, std::string(), base::string16(), false, 0,
        std::string(), EmptyAccountId());
    profile_shortcut_manager_->CreateProfileShortcut(profile_path);
    task_environment_.RunUntilIdle();
    ValidateProfileShortcut(location, profile_name, profile_path);
  }

  // Posts a task to call ShellUtil::CreateOrUpdateShortcut on the COM thread.
  void PostCreateOrUpdateShortcut(
      const base::Location& location,
      ShellUtil::ShortcutLocation shortcut_location,
      const ShellUtil::ShortcutProperties& properties) {
    base::PostTaskAndReplyWithResult(
        base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock()})
            .get(),
        location,
        base::Bind(&ShellUtil::CreateOrUpdateShortcut, shortcut_location,
                   properties, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS),
        base::Bind([](bool succeeded) { EXPECT_TRUE(succeeded); }));
    task_environment_.RunUntilIdle();
  }

  // Creates a regular (non-profile) desktop shortcut with the given name and
  // returns its path. Fails the test if an error occurs.
  base::FilePath CreateRegularShortcutWithName(
      const base::Location& location,
      const base::string16& shortcut_name) {
    const base::FilePath shortcut_path =
        GetUserShortcutsDirectory().Append(shortcut_name + installer::kLnkExt);
    EXPECT_FALSE(base::PathExists(shortcut_path)) << location.ToString();

    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(GetExePath(), &properties);
    properties.set_shortcut_name(shortcut_name);
    PostCreateOrUpdateShortcut(location, ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                               properties);
    EXPECT_TRUE(base::PathExists(shortcut_path)) << location.ToString();

    return shortcut_path;
  }

  base::FilePath CreateRegularSystemLevelShortcut(
      const base::Location& location) {
    ShellUtil::ShortcutProperties properties(ShellUtil::SYSTEM_LEVEL);
    ShellUtil::AddDefaultShortcutProperties(GetExePath(), &properties);
    PostCreateOrUpdateShortcut(location, ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                               properties);
    const base::FilePath system_level_shortcut_path =
        GetSystemShortcutsDirectory().Append(InstallUtil::GetShortcutName() +
                                             installer::kLnkExt);
    EXPECT_TRUE(base::PathExists(system_level_shortcut_path))
        << location.ToString();
    return system_level_shortcut_path;
  }

  void RenameProfile(const base::Location& location,
                     const base::FilePath& profile_path,
                     const base::string16& new_profile_name) {
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(profile_attributes_storage_->
                    GetProfileAttributesWithPath(profile_path, &entry));
    ASSERT_NE(entry->GetLocalProfileName(), new_profile_name);
    entry->SetLocalProfileName(new_profile_name);
    task_environment_.RunUntilIdle();
  }

  base::FilePath GetExePath() {
    base::FilePath exe_path;
    EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));
    return exe_path;
  }

  base::FilePath GetUserShortcutsDirectory() {
    base::FilePath user_shortcuts_directory;
    EXPECT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           ShellUtil::CURRENT_USER,
                                           &user_shortcuts_directory));
    return user_shortcuts_directory;
  }

  base::FilePath GetSystemShortcutsDirectory() {
    base::FilePath system_shortcuts_directory;
    EXPECT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           ShellUtil::SYSTEM_LEVEL,
                                           &system_shortcuts_directory));
    return system_shortcuts_directory;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ProfileShortcutManager> profile_shortcut_manager_;
  ProfileAttributesStorage* profile_attributes_storage_;
  base::ScopedPathOverride fake_user_desktop_;
  base::ScopedPathOverride fake_system_desktop_;
  base::string16 profile_1_name_;
  base::FilePath profile_1_path_;
  base::string16 profile_2_name_;
  base::FilePath profile_2_path_;
  base::string16 profile_3_name_;
  base::FilePath profile_3_path_;
};

TEST_F(ProfileShortcutManagerTest, ShortcutFilename) {
  const base::string16 kProfileName = L"Harry";
  const base::string16 expected_name = kProfileName + L" - " +
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME) + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName));
}

TEST_F(ProfileShortcutManagerTest, ShortcutLongFilenameIsTrimmed) {
  const base::string16 kLongProfileName =
      L"Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry";
  const base::string16 file_name =
      profiles::internal::GetShortcutFilenameForProfile(kLongProfileName);
  EXPECT_LT(file_name.size(), kLongProfileName.size());
}

TEST_F(ProfileShortcutManagerTest, ShortcutFilenameStripsReservedCharacters) {
  const base::string16 kProfileName = L"<Harry/>";
  const base::string16 kSanitizedProfileName = L"Harry";
  const base::string16 expected_name = kSanitizedProfileName + L" - " +
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME) + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName));
}

TEST_F(ProfileShortcutManagerTest, UnbadgedShortcutFilename) {
  EXPECT_EQ(
      InstallUtil::GetShortcutName() + installer::kLnkExt,
      profiles::internal::GetShortcutFilenameForProfile(base::string16()));
}

TEST_F(ProfileShortcutManagerTest, ShortcutFlags) {
  const base::string16 kProfileName = L"MyProfileX";
  const base::FilePath profile_path =
      profile_manager_->profiles_dir().Append(kProfileName);
  EXPECT_EQ(L"--profile-directory=\"" + kProfileName + L"\"",
            profiles::internal::CreateProfileShortcutFlags(profile_path));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreate) {
  SetupDefaultProfileShortcut(FROM_HERE);
  // Validation is done by |ValidateProfileShortcutAtPath()| which is called
  // by |CreateProfileWithShortcut()|.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsUpdate) {
  SetupDefaultProfileShortcut(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Cause an update in ProfileShortcutManager by modifying the profile info
  // cache.
  const base::string16 new_profile_2_name = L"New Profile Name";
  RenameProfile(FROM_HERE, profile_2_path_, new_profile_2_name);
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, CreateSecondProfileBadgesFirstShortcut) {
  SetupDefaultProfileShortcut(FROM_HERE);
  // Assert that a shortcut without a profile name exists.
  ASSERT_TRUE(ProfileShortcutExistsAtDefaultPath(base::string16()));

  // Create a second profile without a shortcut.
  profile_attributes_storage_->AddProfile(
      profile_2_path_, profile_2_name_, std::string(), base::string16(), false,
      0, std::string(), EmptyAccountId());
  task_environment_.RunUntilIdle();

  // Ensure that the second profile doesn't have a shortcut and that the first
  // profile's shortcut got renamed and badged.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(base::string16()));
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

// Test that adding a second profile, when the first profile has a non-profile
// shortcut, updates the non-profile shortcut to be a badged profile shortcut.
// This happens if the user has one profile, created with a version of Chrome
// that creates non-profile shortcuts, upgrades to a newer version of Chrome
// that creates default profile shortcuts, and adds a second profile.
TEST_F(ProfileShortcutManagerTest, NonProfileShortcutUpgrade) {
  SetupTwoProfilesAndNonProfileShortcut(FROM_HERE);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsDeleteSecondToLast) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  // Delete one shortcut.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));

  // Verify that the remaining shortcut points at the remaining profile.
  ValidateSingleProfileShortcut(FROM_HERE, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, DeleteSecondToLastProfileWithoutShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(base::DeleteFile(profile_1_shortcut_path, false));
  ASSERT_FALSE(base::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(base::PathExists(profile_2_shortcut_path));

  // Delete the profile that doesn't have a shortcut.
  profile_attributes_storage_->RemoveProfile(profile_1_path_);
  task_environment_.RunUntilIdle();

  // Verify that the remaining shortcut does not have a profile name.
  ValidateSingleProfileShortcut(FROM_HERE, profile_2_path_);
  // Verify that shortcuts with profile names do not exist.
  EXPECT_FALSE(base::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DeleteSecondToLastProfileWithShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(base::DeleteFile(profile_1_shortcut_path, false));
  ASSERT_FALSE(base::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(base::PathExists(profile_2_shortcut_path));

  // Delete the profile that has a shortcut.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();

  // Verify that the remaining shortcut is a single profile shortcut.
  ValidateSingleProfileShortcut(FROM_HERE, profile_1_path_);
  // Verify that shortcuts with profile names do not exist.
  EXPECT_FALSE(base::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DeleteOnlyProfileWithShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_3_name_, profile_3_path_);

  const base::FilePath non_profile_shortcut_path =
      GetDefaultShortcutPathForProfile(base::string16());
  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_3_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_3_name_);

  // Delete shortcuts for the first two profiles.
  ASSERT_TRUE(base::DeleteFile(profile_1_shortcut_path, false));
  ASSERT_TRUE(base::DeleteFile(profile_2_shortcut_path, false));

  // Only the shortcut to the third profile should exist.
  ASSERT_FALSE(base::PathExists(profile_1_shortcut_path));
  ASSERT_FALSE(base::PathExists(profile_2_shortcut_path));
  ASSERT_FALSE(base::PathExists(non_profile_shortcut_path));
  ASSERT_TRUE(base::PathExists(profile_3_shortcut_path));

  // Delete the third profile and check that its shortcut is gone and no
  // shortcuts have been re-created.
  profile_attributes_storage_->RemoveProfile(profile_3_path_);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(base::PathExists(profile_1_shortcut_path));
  ASSERT_FALSE(base::PathExists(profile_2_shortcut_path));
  ASSERT_FALSE(base::PathExists(profile_3_shortcut_path));
  ASSERT_FALSE(base::PathExists(non_profile_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreateSecond) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  // Delete one shortcut.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();

  // Verify that an unbadged, default named shortcut for profile1 exists.
  ValidateSingleProfileShortcut(FROM_HERE, profile_1_path_);
  // Verify that an additional shortcut, with the first profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));

  // Create a second profile and shortcut.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Verify that the original shortcut received the profile's name.
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
  // Verify that a default shortcut no longer exists.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(base::string16()));
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  ASSERT_TRUE(base::Move(profile_2_shortcut_path_1,
                              profile_2_shortcut_path_2));

  // Ensure that a new shortcut does not get made if the old one was renamed.
  profile_shortcut_manager_->CreateProfileShortcut(profile_2_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Delete the renamed shortcut and try to create it again, which should work.
  ASSERT_TRUE(base::DeleteFile(profile_2_shortcut_path_2, false));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path_2));
  profile_shortcut_manager_->CreateProfileShortcut(profile_2_path_);
  task_environment_.RunUntilIdle();
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsGetDeleted) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(base::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Also, copy the shortcut for the first user and ensure it gets preserved.
  const base::FilePath preserved_profile_1_shortcut_path =
      GetUserShortcutsDirectory().Append(L"Preserved.lnk");
  ASSERT_TRUE(base::CopyFile(
      GetDefaultShortcutPathForProfile(profile_1_name_),
      preserved_profile_1_shortcut_path));
  EXPECT_TRUE(base::PathExists(preserved_profile_1_shortcut_path));

  // Delete the profile and ensure both shortcuts were also deleted.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path_1));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path_2));
  ValidateSingleProfileShortcutAtPath(FROM_HERE, profile_1_path_,
                                      preserved_profile_1_shortcut_path);
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsAfterProfileRename) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(base::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Now, rename the profile.
  const base::string16 new_profile_2_name = L"New profile";
  RenameProfile(FROM_HERE, profile_2_path_, new_profile_2_name);

  // The original shortcut should be renamed but the copied shortcut should
  // keep its name.
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path_1));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, UpdateShortcutWithNoFlags) {
  SetupDefaultProfileShortcut(FROM_HERE);

  // Delete the shortcut that got created for this profile and instead make
  // a new one without any command-line flags.
  ASSERT_TRUE(base::DeleteFile(
      GetDefaultShortcutPathForProfile(base::string16()), false));
  const base::FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE, InstallUtil::GetShortcutName());

  // Add another profile and check that the shortcut was replaced with
  // a badged shortcut with the right command line for the profile
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_FALSE(base::PathExists(regular_shortcut_path));
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, UpdateTwoShortcutsWithNoFlags) {
  SetupDefaultProfileShortcut(FROM_HERE);

  // Delete the shortcut that got created for this profile and instead make
  // two new ones without any command-line flags.
  ASSERT_TRUE(base::DeleteFile(
      GetDefaultShortcutPathForProfile(base::string16()), false));
  const base::FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE, InstallUtil::GetShortcutName());
  const base::FilePath customized_regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE, L"MyChrome");

  // Add another profile and check that one shortcut was renamed and that the
  // other shortcut was updated but kept the same name.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_FALSE(base::PathExists(regular_shortcut_path));
  ValidateProfileShortcutAtPath(FROM_HERE, customized_regular_shortcut_path,
                                profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, RemoveProfileShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_3_name_, profile_3_path_);

  const base::FilePath profile_1_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Make copies of the shortcuts for both profiles.
  const base::FilePath profile_1_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"Copied1.lnk");
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"Copied2.lnk");
  ASSERT_TRUE(base::CopyFile(profile_1_shortcut_path_1,
                                  profile_1_shortcut_path_2));
  ASSERT_TRUE(base::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_1_shortcut_path_2,
                                profile_1_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Delete shortcuts for profile 1 and ensure that they got deleted while the
  // shortcuts for profile 2 were kept.
  profile_shortcut_manager_->RemoveProfileShortcuts(profile_1_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(profile_1_shortcut_path_1));
  EXPECT_FALSE(base::PathExists(profile_1_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, HasProfileShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  struct HasShortcutsResult {
    bool has_shortcuts;
    void set_has_shortcuts(bool value) { has_shortcuts = value; }
  } result = { false };

  const base::Callback<void(bool)> callback =
      base::Bind(&HasShortcutsResult::set_has_shortcuts,
                 base::Unretained(&result));

  // Profile 2 should have a shortcut initially.
  profile_shortcut_manager_->HasProfileShortcuts(profile_2_path_, callback);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(result.has_shortcuts);

  // Delete the shortcut and check that the function returns false.
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  ASSERT_TRUE(base::DeleteFile(profile_2_shortcut_path, false));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path));
  profile_shortcut_manager_->HasProfileShortcuts(profile_2_path_, callback);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(result.has_shortcuts);
}

TEST_F(ProfileShortcutManagerTest, ProfileShortcutsWithSystemLevelShortcut) {
  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Create the initial profile.
  profile_attributes_storage_->AddProfile(
      profile_1_path_, profile_1_name_, std::string(), base::string16(), false,
      0, std::string(), EmptyAccountId());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1u, profile_attributes_storage_->GetNumberOfProfiles());

  // Ensure system-level continues to exist and user-level was not created.
  EXPECT_TRUE(base::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(base::PathExists(
                   GetDefaultShortcutPathForProfile(base::string16())));

  // Create another profile with a shortcut and ensure both profiles receive
  // user-level profile shortcuts and the system-level one still exists.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_TRUE(base::PathExists(system_level_shortcut_path));

  // Create a third profile without a shortcut and ensure it doesn't get one.
  profile_attributes_storage_->AddProfile(
      profile_3_path_, profile_3_name_, std::string(), base::string16(), false,
      0, std::string(), EmptyAccountId());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));

  // Ensure that changing the avatar icon and the name does not result in a
  // shortcut being created.
  ProfileAttributesEntry* entry_3;
  ASSERT_TRUE(profile_attributes_storage_->
                  GetProfileAttributesWithPath(profile_3_path_, &entry_3));
  entry_3->SetAvatarIconIndex(3u);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));

  const base::string16 new_profile_3_name = L"New Name 3";
  entry_3->SetLocalProfileName(new_profile_3_name);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(new_profile_3_name));

  // Rename the second profile and ensure its shortcut got renamed.
  const base::string16 new_profile_2_name = L"New Name 2";
  ProfileAttributesEntry* entry_2;
  ASSERT_TRUE(profile_attributes_storage_->
                  GetProfileAttributesWithPath(profile_2_path_, &entry_2));
  entry_2->SetLocalProfileName(new_profile_2_name);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest,
       DeleteSecondToLastProfileWithSystemLevelShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Delete a profile and verify that only the system-level shortcut still
  // exists.
  profile_attributes_storage_->RemoveProfile(profile_1_path_);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(base::string16()));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
}

TEST_F(ProfileShortcutManagerTest,
       DeleteSecondToLastProfileWithShortcutWhenSystemLevelShortcutExists) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(base::DeleteFile(profile_1_shortcut_path, false));
  ASSERT_FALSE(base::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(base::PathExists(profile_2_shortcut_path));

  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Delete the profile that has a shortcut, which will exercise the non-profile
  // shortcut creation path in |DeleteDesktopShortcuts()|, which is
  // not covered by the |DeleteSecondToLastProfileWithSystemLevelShortcut| test.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();

  // Verify that only the system-level shortcut still exists.
  EXPECT_TRUE(base::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(base::PathExists(
                   GetDefaultShortcutPathForProfile(base::string16())));
  EXPECT_FALSE(base::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(base::PathExists(profile_2_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, CreateProfileIcon) {
  SetupDefaultProfileShortcut(FROM_HERE);

  const base::FilePath icon_path =
      profiles::internal::GetProfileIconPath(profile_1_path_);

  EXPECT_TRUE(base::PathExists(icon_path));
  EXPECT_TRUE(base::DeleteFile(icon_path, false));
  EXPECT_FALSE(base::PathExists(icon_path));

  profile_shortcut_manager_->CreateOrUpdateProfileIcon(profile_1_path_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(icon_path));
}

TEST_F(ProfileShortcutManagerTest, UnbadgeProfileIconOnDeletion) {
  SetupDefaultProfileShortcut(FROM_HERE);
  const base::FilePath icon_path_1 =
      profiles::internal::GetProfileIconPath(profile_1_path_);
  const base::FilePath icon_path_2 =
      profiles::internal::GetProfileIconPath(profile_2_path_);

  // Default profile has unbadged icon to start.
  std::string unbadged_icon_1;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &unbadged_icon_1));

  // Creating a new profile adds a badge to both the new profile icon and the
  // default profile icon. Since they use the same icon index, the icon files
  // should be the same.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  std::string badged_icon_1;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &badged_icon_1));
  std::string badged_icon_2;
  EXPECT_TRUE(base::ReadFileToString(icon_path_2, &badged_icon_2));

  EXPECT_NE(badged_icon_1, unbadged_icon_1);
  EXPECT_EQ(badged_icon_1, badged_icon_2);

  // Deleting the default profile will unbadge the new profile's icon and should
  // result in an icon that is identical to the unbadged default profile icon.
  profile_attributes_storage_->RemoveProfile(profile_1_path_);
  task_environment_.RunUntilIdle();

  std::string unbadged_icon_2;
  EXPECT_TRUE(base::ReadFileToString(icon_path_2, &unbadged_icon_2));
  EXPECT_EQ(unbadged_icon_1, unbadged_icon_2);
}

TEST_F(ProfileShortcutManagerTest, ProfileIconOnAvatarChange) {
  SetupAndCreateTwoShortcuts(FROM_HERE);
  const base::FilePath icon_path_1 =
      profiles::internal::GetProfileIconPath(profile_1_path_);
  const base::FilePath icon_path_2 =
      profiles::internal::GetProfileIconPath(profile_2_path_);

  std::string badged_icon_1;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &badged_icon_1));
  std::string badged_icon_2;
  EXPECT_TRUE(base::ReadFileToString(icon_path_2, &badged_icon_2));

  // Profile 1 and 2 are created with the same icon.
  EXPECT_EQ(badged_icon_1, badged_icon_2);

  // Change profile 1's icon.
  ProfileAttributesEntry* entry_1;
  ASSERT_TRUE(profile_attributes_storage_->
                  GetProfileAttributesWithPath(profile_1_path_, &entry_1));
  entry_1->SetAvatarIconIndex(1u);
  task_environment_.RunUntilIdle();

  std::string new_badged_icon_1;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &new_badged_icon_1));
  EXPECT_NE(new_badged_icon_1, badged_icon_1);

  // Ensure the new icon is not the unbadged icon.
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();

  std::string unbadged_icon_1;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &unbadged_icon_1));
  EXPECT_NE(unbadged_icon_1, new_badged_icon_1);

  // Ensure the icon doesn't change on avatar change without 2 profiles.
  entry_1->SetAvatarIconIndex(1u);
  task_environment_.RunUntilIdle();

  std::string unbadged_icon_1_a;
  EXPECT_TRUE(base::ReadFileToString(icon_path_1, &unbadged_icon_1_a));
  EXPECT_EQ(unbadged_icon_1, unbadged_icon_1_a);
}

TEST_F(ProfileShortcutManagerTest, ShortcutFilenameUniquified) {
  const auto suffix = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  std::set<base::FilePath> excludes;

  base::string16 shortcut_filename =
      profiles::internal::GetUniqueShortcutFilenameForProfile(L"Carrie",
                                                              excludes);
  EXPECT_EQ(
      L"Carrie - " + suffix + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Carrie", excludes);
  EXPECT_EQ(
      L"Carrie - " + suffix + L" (1)" + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Carrie", excludes);
  EXPECT_EQ(
      L"Carrie - " + suffix + L" (2)" + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Steven", excludes);
  EXPECT_EQ(
      L"Steven - " + suffix + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Steven", excludes);
  EXPECT_EQ(
      L"Steven - " + suffix + L" (1)" + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Carrie", excludes);
  EXPECT_EQ(
      L"Carrie - " + suffix + L" (3)" + installer::kLnkExt, shortcut_filename);
  excludes.insert(GetUserShortcutsDirectory().Append(shortcut_filename));

  excludes.erase(
      GetUserShortcutsDirectory().Append(
          L"Carrie - " + suffix + installer::kLnkExt));
  shortcut_filename = profiles::internal::GetUniqueShortcutFilenameForProfile(
      L"Carrie", excludes);
  EXPECT_EQ(
      L"Carrie - " + suffix + installer::kLnkExt, shortcut_filename);
}

TEST_F(ProfileShortcutManagerTest, ShortcutFilenameMatcher) {
  profiles::internal::ShortcutFilenameMatcher matcher(L"Carrie");
  const auto suffix = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);

  EXPECT_TRUE(matcher.IsCanonical(L"Carrie - " + suffix + L" (2)" +
                                  installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L"(2)" +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L" 2" +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L"2" +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L" - 2" +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L" (a)" +
                                   installer::kLnkExt));
  EXPECT_TRUE(matcher.IsCanonical(L"Carrie - " + suffix + L" (11)" +
                                  installer::kLnkExt));
  EXPECT_TRUE(matcher.IsCanonical(L"Carrie - " + suffix + L" (03)" +
                                  installer::kLnkExt));
  EXPECT_TRUE(matcher.IsCanonical(L"Carrie - " + suffix + L" (999)" +
                                  installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(L"Carrie - " + suffix + L" (999).lin"));
  EXPECT_FALSE(matcher.IsCanonical(L"ABC Carrie - " + suffix + L" DEF" +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(base::string16(L"ABC Carrie DEF") +
                                   installer::kLnkExt));
  EXPECT_FALSE(matcher.IsCanonical(base::string16(L"Carrie") +
                                   installer::kLnkExt));
}

TEST_F(ProfileShortcutManagerTest, ShortcutsForProfilesWithIdenticalNames) {
  SetupDefaultProfileShortcut(FROM_HERE);

  // Create new profile - profile2.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  // Check that nothing is changed for profile1.
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);

  // Give to profile1 the same name as profile2.
  base::string16 new_profile_1_name = profile_2_name_;
  RenameProfile(FROM_HERE, profile_1_path_, new_profile_1_name);
  const auto profile_1_shortcut_name = new_profile_1_name + L" - " +
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME) + L" (1)";
  const auto profile_1_shortcut_path = GetUserShortcutsDirectory()
      .Append(profile_1_shortcut_name + installer::kLnkExt);
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_1_shortcut_path,
                                profile_1_path_);
  // Check that nothing is changed for profile2.
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Create new profile - profile3.
  CreateProfileWithShortcut(FROM_HERE, profile_3_name_, profile_3_path_);
  // Check that nothing is changed for profile1 and profile2.
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_1_shortcut_path,
                                profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Give to profile3 the same name as profile2.
  const base::string16 new_profile_3_name = profile_2_name_;
  RenameProfile(FROM_HERE, profile_3_path_, new_profile_3_name);
  const auto profile_3_shortcut_name = new_profile_3_name + L" - " +
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME) + L" (2)";
  const auto profile_3_shortcut_path = GetUserShortcutsDirectory()
      .Append(profile_3_shortcut_name + installer::kLnkExt);
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_3_shortcut_path,
                                profile_3_path_);
  // Check that nothing is changed for profile1 and profile2.
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_1_shortcut_path,
                                profile_1_path_);

  // Rename profile1 again.
  new_profile_1_name = L"Carrie";
  RenameProfile(FROM_HERE, profile_1_path_, new_profile_1_name);
  ValidateProfileShortcut(FROM_HERE, new_profile_1_name, profile_1_path_);
  // Check that nothing is changed for profile2 and profile3.
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_3_shortcut_path,
                                profile_3_path_);

  // Delete profile1.
  profile_attributes_storage_->RemoveProfile(profile_1_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(new_profile_1_name));
  // Check that nothing is changed for profile2 and profile3.
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE,
                                profile_3_shortcut_path,
                                profile_3_path_);

  // Delete profile2.
  EXPECT_TRUE(base::PathExists(
      GetDefaultShortcutPathForProfile(profile_2_name_)));
  EXPECT_TRUE(base::PathExists(profile_3_shortcut_path));
  profile_attributes_storage_->RemoveProfile(profile_2_path_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(
      GetDefaultShortcutPathForProfile(profile_2_name_)));
  // Only profile3 exists. There should be a single profile shortcut only.
  EXPECT_FALSE(base::PathExists(profile_3_shortcut_path));
  ValidateSingleProfileShortcut(FROM_HERE, profile_3_path_);
}
