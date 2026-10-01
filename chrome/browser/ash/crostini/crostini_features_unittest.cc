// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_features.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

TEST(CrostiniFeaturesTest, TestFakeReplaces) {
  CrostiniFeatures* original = CrostiniFeatures::Get();
  {
    FakeCrostiniFeatures crostini_features;
    EXPECT_NE(original, CrostiniFeatures::Get());
    EXPECT_EQ(&crostini_features, CrostiniFeatures::Get());
  }
  EXPECT_EQ(original, CrostiniFeatures::Get());
}

TEST(CrostiniFeaturesTest, TestExportImportUIAllowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Set up for success.
  crostini_features.set_is_allowed_now(true);
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, true);

  // Success.
  EXPECT_TRUE(crostini_features.IsExportImportUIAllowed(&profile));

  // Crostini UI not allowed.
  crostini_features.set_is_allowed_now(false);
  EXPECT_FALSE(crostini_features.IsExportImportUIAllowed(&profile));
  crostini_features.set_is_allowed_now(true);

  // Pref off.
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, false);
  EXPECT_FALSE(crostini_features.IsExportImportUIAllowed(&profile));
}

TEST(CrostiniFeaturesTest, TestRootAccessAllowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;
  base::test::ScopedFeatureList scoped_feature_list;

  // Set up for success.
  crostini_features.set_is_allowed_now(true);
  scoped_feature_list.InitWithFeatures(
      {ash::features::kCrostiniAdvancedAccessControls}, {});
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy, true);

  // Success.
  EXPECT_TRUE(crostini_features.IsRootAccessAllowed(&profile));

  // Pref off.
  profile.GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy, false);
  EXPECT_FALSE(crostini_features.IsRootAccessAllowed(&profile));

  // Feature disabled.
  {
    base::test::ScopedFeatureList feature_list_disabled;
    feature_list_disabled.InitWithFeatures(
        {}, {ash::features::kCrostiniAdvancedAccessControls});
    EXPECT_TRUE(crostini_features.IsRootAccessAllowed(&profile));
  }
}

class CrostiniFeaturesAllowedTest : public testing::Test {
 protected:
  CrostiniFeaturesAllowedTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kCrostini}, {});
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override {
    profile_ = nullptr;
    user_session_test_environment_.reset();
  }

  void AddUserWithAffiliation(bool is_affiliated) {
    const user_manager::User* user =
        user_session_test_environment_->AddRegularUser(
            AccountId::FromUserEmailGaiaId("test@example.com",
                                           GaiaId("1234567890")));
    user_session_test_environment_->LogIn(user->GetAccountId());
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        user->GetAccountId(), /*is_managed=*/is_affiliated, is_affiliated);
    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  FakeCrostiniFeatures crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrostiniFeaturesAllowedTest, TestDefaultUnmanagedBehaviour) {
  AddUserWithAffiliation(false);

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(profile_, &reason);
  EXPECT_TRUE(crostini_is_allowed_now);
}

TEST_F(CrostiniFeaturesAllowedTest, TestDefaultAffiliatedUserBehaviour) {
  AddUserWithAffiliation(true);

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(profile_, &reason);
  EXPECT_FALSE(crostini_is_allowed_now);
  EXPECT_EQ(reason,
            "Affiliated user is not allowed to run Crostini by default.");
}

TEST_F(CrostiniFeaturesAllowedTest, TestPolicyAffiliatedUserBehaviour) {
  AddUserWithAffiliation(true);
  profile_->GetTestingPrefService()->SetManagedPref(
      crostini::prefs::kUserCrostiniAllowedByPolicy,
      std::make_unique<base::Value>(true));

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(profile_, &reason);
  EXPECT_TRUE(crostini_is_allowed_now);
}

class CrostiniFeaturesAdbSideloadingTest : public testing::Test {
 protected:
  CrostiniFeaturesAdbSideloadingTest() = default;

  void SetUp() override {
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override {
    profile_ = nullptr;
    user_session_test_environment_.reset();
  }

  void AddChildUser() {
    const user_manager::User* user =
        user_session_test_environment_->AddChildUser(
            AccountId::FromUserEmailGaiaId("test@example.com",
                                           GaiaId("1234567890")));
    user_session_test_environment_->LogIn(user->GetAccountId());
    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
    profile_->SetIsSupervisedProfile();
  }

  void AddOwnerUser() {
    const user_manager::User* user =
        user_session_test_environment_->AddRegularUser(
            AccountId::FromUserEmailGaiaId("test@example.com",
                                           GaiaId("1234567890")));
    user_session_test_environment_->LogIn(user->GetAccountId());
    user_manager::UserManager::Get()->SetOwnerId(user->GetAccountId());
    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
  }

  void AddUserWithAffiliation(bool is_affiliated) {
    const user_manager::User* user =
        user_session_test_environment_->AddRegularUser(
            AccountId::FromUserEmailGaiaId("test@example.com",
                                           GaiaId("1234567890")));
    user_session_test_environment_->LogIn(user->GetAccountId());
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        user->GetAccountId(), /*is_managed=*/is_affiliated, is_affiliated);
    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
  }

  void SetManagedUser(bool is_managed) {
    profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
        is_managed);
  }

  void SetDeviceToConsumerOwned() {
    scoped_settings_helper_.InstallAttributes()->SetConsumerOwned();
  }

  void SetDeviceToEnterpriseManaged() {
    scoped_settings_helper_.InstallAttributes()->SetCloudManaged("domain.com",
                                                                 "device_id");
  }

  void AssertCanChangeAdbSideloading(bool expected_can_change) {
    base::test::TestFuture<bool> result_future;
    crostini_features_.CanChangeAdbSideloading(profile_,
                                               result_future.GetCallback());
    EXPECT_EQ(result_future.Get(), expected_can_change);
  }

  content::BrowserTaskEnvironment task_environment_;

  ash::ScopedCrosSettingsTestHelper scoped_settings_helper_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  FakeCrostiniFeatures crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingChildUser) {
  AddChildUser();

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest, TestCanChangeAdbSideloadingManaged) {
  SetDeviceToEnterpriseManaged();
  AddUserWithAffiliation(true);
  SetManagedUser(true);

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingOwnerProfile) {
  SetDeviceToConsumerOwned();
  AddOwnerUser();
  SetManagedUser(false);

  AssertCanChangeAdbSideloading(true);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingOwnerProfileManagedUser) {
  SetDeviceToConsumerOwned();
  AddOwnerUser();
  SetManagedUser(true);

  AssertCanChangeAdbSideloading(false);
}

TEST(CrostiniFeaturesTest, TestPortForwardingAllowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Default case.
  EXPECT_TRUE(crostini_features.IsPortForwardingAllowed(&profile));

  // Set pref to true.
  profile.GetTestingPrefService()->SetManagedPref(
      crostini::prefs::kCrostiniPortForwardingAllowedByPolicy,
      std::make_unique<base::Value>(true));

  // Allowed.
  EXPECT_TRUE(crostini_features.IsPortForwardingAllowed(&profile));
}

TEST(CrostiniFeaturesTest, TestPortForwardingDisallowed) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeCrostiniFeatures crostini_features;

  // Set pref to false.
  profile.GetTestingPrefService()->SetManagedPref(
      crostini::prefs::kCrostiniPortForwardingAllowedByPolicy,
      std::make_unique<base::Value>(false));

  // Disallowed.
  EXPECT_FALSE(crostini_features.IsPortForwardingAllowed(&profile));
}

}  // namespace crostini
