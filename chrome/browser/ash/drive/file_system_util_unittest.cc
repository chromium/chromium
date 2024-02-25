// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive::util {
namespace {

using ash::features::kDriveFsBulkPinning;
using ash::features::kFeatureManagementDriveFsBulkPinning;
using base::test::ScopedFeatureList;

// Marks the current thread as UI by BrowserTaskEnvironment. We need the task
// environment since Profile objects must be touched from UI and hence has
// CHECK/DCHECKs for it.
class ProfileRelatedFileSystemUtilTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(ProfileRelatedFileSystemUtilTest, IsUnderDriveMountPoint) {
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("media/fuse/drivefs/foo.txt")));

  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs/subdir/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/media/fuse/drivefs-xxx/foo.txt")));
}

TEST_F(ProfileRelatedFileSystemUtilTest, GetCacheRootPath) {
  TestingProfile profile;
  base::FilePath profile_path = profile.GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(&profile));
}

TEST_F(ProfileRelatedFileSystemUtilTest, SetDriveConnectionStatusForTesting) {
  TestingProfile profile;
  using enum ConnectionStatus;
  EXPECT_EQ(GetDriveConnectionStatus(&profile), kNoService);

  for (const ConnectionStatus status :
       {kNoNetwork, kNotReady, kNoService, kMetered, kConnected}) {
    SetDriveConnectionStatusForTesting(status);
    EXPECT_EQ(GetDriveConnectionStatus(&profile), status);
  }
}

TEST_F(ProfileRelatedFileSystemUtilTest, IsDriveFsBulkPinningAvailable) {
  TestingProfile profile;
  PrefService* const prefs = profile.GetPrefs();
  DCHECK(prefs);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kDriveFsBulkPinningVisible));

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable());
  }

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kFeatureManagementDriveFsBulkPinning},
                              {kDriveFsBulkPinning});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable());
  }

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kDriveFsBulkPinning},
                              {kFeatureManagementDriveFsBulkPinning});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable());
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, false);

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, true);

  {
    ScopedFeatureList features;
    features.InitWithFeatures(
        {kFeatureManagementDriveFsBulkPinning, kDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(&profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }

  // Test for Googler account.
  {
    ScopedFeatureList features;
    features.InitWithFeatures({kDriveFsBulkPinning},
                              {kFeatureManagementDriveFsBulkPinning});

    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(&profile));

    const user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
        user_manager(std::make_unique<ash::FakeChromeUserManager>());
    user_manager->AddUser(AccountId::FromUserEmailGaiaId(
        "foobar@google.com", FakeGaiaMixin::kEnterpriseUser1GaiaId));

    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(&profile));
  }
}

TEST_F(ProfileRelatedFileSystemUtilTest,
       CheckDriveEnabledAndDriveAvailabilityForProfile) {
  TestingProfile profile;
  PrefService* const prefs = profile.GetPrefs();
  DCHECK(prefs);

  // Set disable Drive preference to true.
  prefs->SetBoolean(prefs::kDisableDrive, true);

  // Check kNotAvailableWhenDisableDrivePreferenceSet.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(&profile),
            DriveAvailability::kNotAvailableWhenDisableDrivePreferenceSet);

  // Set disable Drive preference to false.
  prefs->SetBoolean(prefs::kDisableDrive, false);

  // Check kNotAvailableForUninitialisedLoginState.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(&profile),
            DriveAvailability::kNotAvailableForUninitialisedLoginState);

  // Initialise login state.
  ash::LoginState::Initialize();

  // Check kNotAvailableForAccountType.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(&profile),
            DriveAvailability::kNotAvailableForAccountType);

  // Login gaia user.
  const user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager(std::make_unique<ash::FakeChromeUserManager>());
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      "foobar@google.com", FakeGaiaMixin::kEnterpriseUser1GaiaId));
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      user_manager->GetPrimaryUser(), &profile);

  // Check kAvailable.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(&profile),
            DriveAvailability::kAvailable);

  // Get incognito profile.
  Profile* incongnito_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  // Check kNotAvailableInIncognito.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(incongnito_profile),
            DriveAvailability::kNotAvailableInIncognito);
}

}  // namespace drive::util
