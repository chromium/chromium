// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/file_type_policies.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/drive/drive_pref_names.h"
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

// Verifies prefs are registered correctly.
TEST(DownloadPrefsTest, RegisterPrefs) {
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester;

#ifdef OS_ANDROID
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(download::features::kDownloadLater);
#endif  // OS_ANDROID

  // Download prefs are registered when creating the profile.
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

#ifdef OS_ANDROID
  // Download prompt prefs should be registered correctly.
  histogram_tester.ExpectBucketCount("MobileDownload.DownloadPromptStatus",
                                     DownloadPromptStatus::SHOW_INITIAL, 1);
  histogram_tester.ExpectBucketCount("MobileDownload.DownloadLaterPromptStatus",
                                     DownloadPromptStatus::SHOW_INITIAL, 1);
  int prompt_status = profile.GetTestingPrefService()->GetInteger(
      prefs::kPromptForDownloadAndroid);
  EXPECT_EQ(prompt_status,
            static_cast<int>(DownloadPromptStatus::SHOW_INITIAL));

  int download_later_prompt_status =
      profile.GetTestingPrefService()->GetInteger(
          prefs::kDownloadLaterPromptStatus);
  EXPECT_EQ(download_later_prompt_status,
            static_cast<int>(DownloadLaterPromptStatus::kShowInitial));
#endif  // OS_ANDROID
}

TEST(DownloadPrefsTest, NoAutoOpenByUserForDisallowedFileTypes) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.EnableAutoOpenByUserBasedOnExtension(kDangerousFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, NoAutoOpenByUserForFilesWithNoExtension) {
  const base::FilePath kFileWithNoExtension(FILE_PATH_LITERAL("abcd"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(
      prefs.EnableAutoOpenByUserBasedOnExtension(kFileWithNoExtension));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kFileWithNoExtension));
}

TEST(DownloadPrefsTest, AutoOpenForSafeFiles) {
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));
  const base::FilePath kAnotherSafeFilePath(
      FILE_PATH_LITERAL("/ok/not-bad.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.EnableAutoOpenByUserBasedOnExtension(kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kAnotherSafeFilePath));
}

TEST(DownloadPrefsTest, AutoOpenPrefSkipsDangerousFileTypesInPrefs) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  // This sets .swf files and .txt files as auto-open file types.
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "swf:txt");
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kSafeFilePath));
}

TEST(DownloadPrefsTest, PrefsInitializationSkipsInvalidFileTypes) {
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "swf:txt::.foo:baz");
  DownloadPrefs prefs(&profile);
  prefs.DisableAutoOpenByUserBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.baz")));

  EXPECT_FALSE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.swf"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.foo"))));

  // .swf is skipped because it's not an allowed auto-open file type.
  // The empty entry and .foo are skipped because they are malformed.
  // "baz" is removed by the DisableAutoOpenByUserBasedOnExtension() call.
  // The only entry that should be remaining is 'txt'.
  EXPECT_STREQ(
      "txt",
      profile.GetPrefs()->GetString(prefs::kDownloadExtensionsToOpen).c_str());
}

TEST(DownloadPrefsTest, AutoOpenCheckIsCaseInsensitive) {
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "txt:Foo:BAR");
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.TXT"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.foo"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.Bar"))));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicy) {
  const base::FilePath kBasicFilePath(
      FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ListPrefUpdate update(profile.GetPrefs(),
                        prefs::kDownloadExtensionsToOpenByPolicy);
  update->AppendString("txt");
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kBasicFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kBasicFilePath));
}

TEST(DownloadPrefsTest, IsAutoOpenByPolicy) {
  const base::FilePath kFilePathType1(
      FILE_PATH_LITERAL("/good/basic-path.txt"));
  const base::FilePath kFilePathType2(
      FILE_PATH_LITERAL("/good/basic-path.exe"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ListPrefUpdate update(profile.GetPrefs(),
                        prefs::kDownloadExtensionsToOpenByPolicy);
  update->AppendString("exe");
  DownloadPrefs prefs(&profile);
  EXPECT_TRUE(prefs.EnableAutoOpenByUserBasedOnExtension(kFilePathType1));

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kFilePathType1));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kFilePathType2));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kURL, kFilePathType1));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kFilePathType2));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyDangerousType) {
  const base::FilePath kDangerousFilePath(
      FILE_PATH_LITERAL("/good/dangerout-type.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ListPrefUpdate update(profile.GetPrefs(),
                        prefs::kDownloadExtensionsToOpenByPolicy);
  update->AppendString("swf");
  DownloadPrefs prefs(&profile);

  // Verifies that the user can't set this file type to auto-open, but it can
  // still be set by policy.
  EXPECT_FALSE(prefs.EnableAutoOpenByUserBasedOnExtension(kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyDynamicUpdates) {
  const base::FilePath kDangerousFilePath(
      FILE_PATH_LITERAL("/good/dangerout-type.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  // Ensure the file won't open open at first, but that it can be as soon as
  // the preference is updated.
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));

  // Update the policy preference.
  {
    ListPrefUpdate update(profile.GetPrefs(),
                          prefs::kDownloadExtensionsToOpenByPolicy);
    update->Append("swf");
  }
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));

  // Remove the policy and ensure the file stops auto-opening.
  {
    ListPrefUpdate update(profile.GetPrefs(),
                          prefs::kDownloadExtensionsToOpenByPolicy);
    update->ClearList();
  }
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyAllowedURLs) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kAllowedURL("http://basic.com");
  const GURL kDisallowedURL("http://disallowed.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ListPrefUpdate update_type(profile.GetPrefs(),
                             prefs::kDownloadExtensionsToOpenByPolicy);
  update_type->AppendString("txt");
  ListPrefUpdate update_url(profile.GetPrefs(),
                            prefs::kDownloadAllowedURLsForOpenByPolicy);
  update_url->AppendString("basic.com");
  DownloadPrefs prefs(&profile);

  // Verifies that the file only opens for the allowed url.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyAllowedURLsDynamicUpdates) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kAllowedURL("http://basic.com");
  const GURL kDisallowedURL("http://disallowed.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ListPrefUpdate update_type(profile.GetPrefs(),
                             prefs::kDownloadExtensionsToOpenByPolicy);
  update_type->AppendString("txt");
  DownloadPrefs prefs(&profile);

  // Ensure both urls work when no restrictions are present.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));

  // Update the policy preference to only allow |kAllowedURL|.
  {
    ListPrefUpdate update_url(profile.GetPrefs(),
                              prefs::kDownloadAllowedURLsForOpenByPolicy);
    update_url->AppendString("basic.com");
  }

  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));

  // Remove the policy and ensure both auto-open again.
  {
    ListPrefUpdate update_url(profile.GetPrefs(),
                              prefs::kDownloadAllowedURLsForOpenByPolicy);
    update_url->ClearList();
  }
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
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
#endif  // OS_CHROMEOS

#ifdef OS_ANDROID
TEST(DownloadPrefsTest, DownloadLaterPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(download::features::kDownloadLater);

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.PromptDownloadLater());
  EXPECT_FALSE(prefs.HasDownloadLaterPromptShown());

  profile.GetPrefs()->SetInteger(
      prefs::kDownloadLaterPromptStatus,
      static_cast<int>(DownloadLaterPromptStatus::kShowPreference));
  EXPECT_TRUE(prefs.PromptDownloadLater());
  EXPECT_TRUE(prefs.HasDownloadLaterPromptShown());

  profile.GetPrefs()->SetInteger(
      prefs::kDownloadLaterPromptStatus,
      static_cast<int>(DownloadLaterPromptStatus::kDontShow));
  EXPECT_FALSE(prefs.PromptDownloadLater());
  EXPECT_TRUE(prefs.HasDownloadLaterPromptShown());
}

#endif  // OS_ANDROID
