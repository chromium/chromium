// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/file_system_util.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive::util {
namespace {

using ash::features::kFeatureManagementDriveFsBulkPinning;
using base::test::ScopedFeatureList;

// Marks the current thread as UI by BrowserTaskEnvironment. We need the task
// environment since Profile objects must be touched from UI and hence has
// CHECK/DCHECKs for it.
class ProfileRelatedFileSystemUtilTest : public testing::Test {
 public:
  void SetUp() override {
    ash::LoginState::Initialize();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override {
    user_session_test_environment_.reset();
    ash::LoginState::Shutdown();
  }

 protected:
  [[nodiscard]] Profile* LogInRegularUser(
      const AccountId& account_id =
          AccountId::FromUserEmailGaiaId("foobar@example.com",
                                         GaiaId("1234567890"))) {
    const user_manager::User* user =
        user_session_test_environment_->AddRegularUser(account_id);
    if (!user) {
      return nullptr;
    }
    user_session_test_environment_->LogIn(account_id);
    return Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
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
  Profile* const profile = LogInRegularUser();
  ASSERT_TRUE(profile);
  base::FilePath profile_path = profile->GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(profile));
}

TEST_F(ProfileRelatedFileSystemUtilTest, SetDriveConnectionStatusForTesting) {
  Profile* const profile = LogInRegularUser();
  ASSERT_TRUE(profile);
  using enum ConnectionStatus;
  EXPECT_EQ(GetDriveConnectionStatus(profile), kNoService);

  for (const ConnectionStatus status :
       {kNoNetwork, kNotReady, kNoService, kMetered, kConnected}) {
    SetDriveConnectionStatusForTesting(status);
    EXPECT_EQ(GetDriveConnectionStatus(profile), status);
  }
}

TEST_F(ProfileRelatedFileSystemUtilTest, IsDriveFsBulkPinningAvailable) {
  Profile* const profile = LogInRegularUser();
  ASSERT_TRUE(profile);
  PrefService* const prefs = profile->GetPrefs();
  DCHECK(prefs);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kDriveFsBulkPinningVisible));

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kFeatureManagementDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable());
  }

  {
    ScopedFeatureList features;
    features.InitWithFeatures({}, {kFeatureManagementDriveFsBulkPinning});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(profile));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable());
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, false);

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kFeatureManagementDriveFsBulkPinning}, {});
    EXPECT_FALSE(IsDriveFsBulkPinningAvailable(profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }

  prefs->SetBoolean(prefs::kDriveFsBulkPinningVisible, true);

  {
    ScopedFeatureList features;
    features.InitWithFeatures({kFeatureManagementDriveFsBulkPinning}, {});
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(profile));
    EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  }
}

TEST_F(ProfileRelatedFileSystemUtilTest,
       IsDriveFsBulkPinningAvailableForGoogler) {
  ScopedFeatureList features;
  features.InitWithFeatures({}, {kFeatureManagementDriveFsBulkPinning});

  EXPECT_FALSE(IsDriveFsBulkPinningAvailable(nullptr));

  Profile* const profile = LogInRegularUser(AccountId::FromUserEmailGaiaId(
      "foobar@google.com", GaiaId(FakeGaiaMixin::kEnterpriseUser1GaiaId)));
  ASSERT_TRUE(profile);

  EXPECT_TRUE(IsDriveFsBulkPinningAvailable(nullptr));
  EXPECT_TRUE(IsDriveFsBulkPinningAvailable(profile));
}

TEST_F(ProfileRelatedFileSystemUtilTest,
       CheckDriveEnabledAndDriveAvailabilityForProfile) {
  Profile* const profile = LogInRegularUser();
  ASSERT_TRUE(profile);
  PrefService* const prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Set disable Drive preference to true.
  prefs->SetBoolean(prefs::kDisableDrive, true);

  // Check kNotAvailableWhenDisableDrivePreferenceSet.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(profile),
            DriveAvailability::kNotAvailableWhenDisableDrivePreferenceSet);

  // Set disable Drive preference to false.
  prefs->SetBoolean(prefs::kDisableDrive, false);

  // Check kAvailable.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(profile),
            DriveAvailability::kAvailable);

  // Get incognito profile.
  Profile* incognito_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  // Check kNotAvailableInIncognito.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(incognito_profile),
            DriveAvailability::kNotAvailableInIncognito);
}

TEST_F(ProfileRelatedFileSystemUtilTest,
       CheckDriveEnabledAndDriveAvailabilityForNonGaiaProfile) {
  const user_manager::User* user =
      user_session_test_environment_->AddPublicAccountUser(
          "public@public-accounts.device-local.localhost");
  ASSERT_TRUE(user);
  user_session_test_environment_->LogIn(user->GetAccountId());
  Profile* const profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  ASSERT_TRUE(profile);

  // Check kNotAvailableForAccountType.
  EXPECT_EQ(CheckDriveEnabledAndDriveAvailabilityForProfile(profile),
            DriveAvailability::kNotAvailableForAccountType);
}

}  // namespace drive::util
