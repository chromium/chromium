// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include "base/files/file_path.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/drive/drive_pref_names.h"
#include "content/public/test/test_service_manager_context.h"
#endif

using safe_browsing::FileTypePolicies;

TEST(DownloadPrefsTest, Prerequisites) {
  // Most of the tests below are based on the assumption that .swf files are not
  // allowed to open automatically, and that .txt files are allowed. If this
  // assumption changes, then we need to update the tests to match.
  ASSERT_FALSE(FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
      base::FilePath(FILE_PATH_LITERAL("a.swf"))));
  ASSERT_TRUE(FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
      base::FilePath(FILE_PATH_LITERAL("a.txt"))));
}

TEST(DownloadPrefsTest, NoAutoOpenForDisallowedFileTypes) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.EnableAutoOpenBasedOnExtension(kDangerousFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenEnabledBasedOnExtension(kDangerousFilePath));
}

TEST(DownloadPrefsTest, NoAutoOpenForFilesWithNoExtension) {
  const base::FilePath kFileWithNoExtension(FILE_PATH_LITERAL("abcd"));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.EnableAutoOpenBasedOnExtension(kFileWithNoExtension));
  EXPECT_FALSE(prefs.IsAutoOpenEnabledBasedOnExtension(kFileWithNoExtension));
}

TEST(DownloadPrefsTest, AutoOpenForSafeFiles) {
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));
  const base::FilePath kAnotherSafeFilePath(
      FILE_PATH_LITERAL("/ok/not-bad.txt"));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.EnableAutoOpenBasedOnExtension(kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(kAnotherSafeFilePath));
}

TEST(DownloadPrefsTest, AutoOpenPrefSkipsDangerousFileTypesInPrefs) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  // This sets .swf files and .txt files as auto-open file types.
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "swf:txt");
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.IsAutoOpenEnabledBasedOnExtension(kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(kSafeFilePath));
}

TEST(DownloadPrefsTest, PrefsInitializationSkipsInvalidFileTypes) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "swf:txt::.foo:baz");
  DownloadPrefs prefs(&profile);
  prefs.DisableAutoOpenBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.baz")));

  EXPECT_FALSE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.swf"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_FALSE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.foo"))));

  // .swf is skipped because it's not an allowed auto-open file type.
  // The empty entry and .foo are skipped because they are malformed.
  // "baz" is removed by the DisableAutoOpenBasedOnExtension() call.
  // The only entry that should be remaining is 'txt'.
  EXPECT_STREQ(
      "txt",
      profile.GetPrefs()->GetString(prefs::kDownloadExtensionsToOpen).c_str());
}

TEST(DownloadPrefsTest, AutoOpenCheckIsCaseInsensitive) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "txt:Foo:BAR");
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.TXT"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.foo"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabledBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.Bar"))));
}

TEST(DownloadPrefsTest, MissingDefaultPathCorrected) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  base::FilePath());
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute())
      << "Default download directory is " << download_prefs.DownloadPath();
}

TEST(DownloadPrefsTest, RelativeDefaultPathCorrected) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;

  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  base::FilePath::FromUTF8Unsafe(".."));
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute())
      << "Default download directory is " << download_prefs.DownloadPath();
}

TEST(DownloadPrefsTest, DefaultPathChangedToInvalidValue) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  profile.GetPath());
  EXPECT_TRUE(profile.GetPrefs()
                  ->GetFilePath(prefs::kDownloadDefaultDirectory)
                  .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute());

  download_prefs.SetDownloadPath(base::FilePath::FromUTF8Unsafe(".."));
  EXPECT_EQ(download_prefs.DownloadPath(),
            download_prefs.GetDefaultDownloadDirectory());
}

#if defined(OS_CHROMEOS)
void ExpectValidDownloadDir(Profile* profile,
                            DownloadPrefs* prefs,
                            base::FilePath path) {
  profile->GetPrefs()->SetString(prefs::kDownloadDefaultDirectory,
                                 path.value());
  EXPECT_TRUE(prefs->DownloadPath().IsAbsolute());
  EXPECT_EQ(prefs->DownloadPath(), path);
}

TEST(DownloadPrefsTest, DownloadDirSanitization) {
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext service_manager_context;
  TestingProfile profile(base::FilePath("/home/chronos/u-0123456789abcdef"));
  DownloadPrefs prefs(&profile);
  const base::FilePath default_dir =
      prefs.GetDefaultDownloadDirectoryForProfile();

  // Test a valid path.
  ExpectValidDownloadDir(&profile, &prefs, default_dir.AppendASCII("testdir"));

  // Test a valid path for Android files.
  ExpectValidDownloadDir(
      &profile, &prefs,
      base::FilePath("/run/arc/sdcard/write/emulated/0/Documents"));

  // Linux files root.
  ExpectValidDownloadDir(
      &profile, &prefs,
      base::FilePath("/media/fuse/crostini_0123456789abcdef_termina_penguin"));
  // Linux files/testdir.
  ExpectValidDownloadDir(
      &profile, &prefs,
      base::FilePath(
          "/media/fuse/crostini_0123456789abcdef_termina_penguin/testdir"));

  // Test with an invalid path outside the download directory.
  profile.GetPrefs()->SetString(prefs::kDownloadDefaultDirectory,
                                "/home/chronos");
  EXPECT_EQ(prefs.DownloadPath(), default_dir);

  // Test with an invalid path containing parent references.
  base::FilePath parent_reference = default_dir.AppendASCII("..");
  profile.GetPrefs()->SetString(prefs::kDownloadDefaultDirectory,
                                parent_reference.value());
  EXPECT_EQ(prefs.DownloadPath(), default_dir);

  // DriveFS
  {
    // Create new profile for enabled feature to work.
    TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
    chromeos::FakeChromeUserManager user_manager;
    DownloadPrefs prefs2(&profile2);
    AccountId account_id =
        AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345");
    const auto* user = user_manager.AddUser(account_id);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &profile2);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        const_cast<user_manager::User*>(user));
    profile2.GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(&profile2);
    integration_service->SetEnabled(true);

    // My Drive root.
    ExpectValidDownloadDir(
        &profile2, &prefs2,
        base::FilePath(
            "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root"));
    // My Drive/foo.
    ExpectValidDownloadDir(
        &profile2, &prefs2,
        base::FilePath(
            "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root/foo"));
    // Invalid path without one of the drive roots.
    const base::FilePath default_dir2 =
        prefs2.GetDefaultDownloadDirectoryForProfile();
    profile2.GetPrefs()->SetString(
        prefs::kDownloadDefaultDirectory,
        "/media/fuse/drivefs-84675c855b63e12f384d45f033826980");
    EXPECT_EQ(prefs2.DownloadPath(), default_dir2);
    profile2.GetPrefs()->SetString(prefs::kDownloadDefaultDirectory,
                                   "/media/fuse/drivefs-something-else/root");
    EXPECT_EQ(prefs2.DownloadPath(), default_dir2);
  }
}
#endif
