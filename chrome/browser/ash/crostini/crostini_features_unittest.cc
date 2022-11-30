// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_features.h"

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
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
      {features::kCrostiniAdvancedAccessControls}, {});
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
        {}, {features::kCrostiniAdvancedAccessControls});
    EXPECT_TRUE(crostini_features.IsRootAccessAllowed(&profile));
  }
}

class CrostiniFeaturesAllowedTest : public testing::Test {
 protected:
  CrostiniFeaturesAllowedTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kCrostini}, {});
  }

  void AddUserWithAffiliation(bool is_affiliated) {
    AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    user_manager_->LoginUser(account_id);
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  FakeCrostiniFeatures crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;

  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(CrostiniFeaturesAllowedTest, TestDefaultUnmanagedBehaviour) {
  AddUserWithAffiliation(false);

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(&profile_, &reason);
  EXPECT_TRUE(crostini_is_allowed_now);
}

TEST_F(CrostiniFeaturesAllowedTest, TestDefaultAffiliatedUserBehaviour) {
  AddUserWithAffiliation(true);

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(&profile_, &reason);
  EXPECT_FALSE(crostini_is_allowed_now);
  EXPECT_EQ(reason,
            "Affiliated user is not allowed to run Crostini by default.");
}

TEST_F(CrostiniFeaturesAllowedTest, TestPolicyAffiliatedUserBehaviour) {
  AddUserWithAffiliation(true);
  profile_.GetTestingPrefService()->SetManagedPref(
      crostini::prefs::kUserCrostiniAllowedByPolicy,
      std::make_unique<base::Value>(true));

  std::string reason;
  bool crostini_is_allowed_now =
      crostini_features_.IsAllowedNow(&profile_, &reason);
  EXPECT_TRUE(crostini_is_allowed_now);
}

class CrostiniFeaturesAdbSideloadingTest : public testing::Test {
 protected:
  CrostiniFeaturesAdbSideloadingTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)) {}

  void SetFeatureFlag(bool is_enabled) {
    if (is_enabled) {
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kArcManagedAdbSideloadingSupport}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {ash::features::kArcManagedAdbSideloadingSupport});
    }
  }

  void AddChildUser() {
    AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    auto* const user = user_manager_->AddChildUser(account_id);
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/true);
  }

  void AddOwnerUser() {
    AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    user_manager_->SetOwnerId(account_id);
  }

  void AddUserWithAffiliation(bool is_affiliated) {
    AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    user_manager_->LoginUser(account_id);
  }

  void SetManagedUser(bool is_managed) {
    profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
        is_managed);
  }

  void SetDeviceToConsumerOwned() {
    profile_.ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetConsumerOwned();
  }

  void SetDeviceToEnterpriseManaged() {
    profile_.ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
  }

  void AllowAdbSideloadingByDevicePolicy() {
    scoped_settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    scoped_settings_helper_.SetInteger(
        ash::kDeviceCrostiniArcAdbSideloadingAllowed,
        enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto::
            ALLOW_FOR_AFFILIATED_USERS);
  }

  void DisallowAdbSideloadingByDevicePolicy() {
    scoped_settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    scoped_settings_helper_.SetInteger(
        ash::kDeviceCrostiniArcAdbSideloadingAllowed,
        enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto::
            DISALLOW);
  }

  void AllowAdbSideloadingByUserPolicy() {
    profile_.GetPrefs()->SetInteger(
        crostini::prefs::kCrostiniArcAdbSideloadingUserPref,
        static_cast<int>(CrostiniArcAdbSideloadingUserAllowanceMode::kAllow));
  }

  void DisallowAdbSideloadingByUserPolicy() {
    profile_.GetPrefs()->SetInteger(
        crostini::prefs::kCrostiniArcAdbSideloadingUserPref,
        static_cast<int>(
            CrostiniArcAdbSideloadingUserAllowanceMode::kDisallow));
  }

  void AssertCanChangeAdbSideloading(bool expected_can_change) {
    base::RunLoop run_loop;
    crostini_features_.CanChangeAdbSideloading(
        &profile_, base::BindLambdaForTesting([&](bool callback_can_change) {
          EXPECT_EQ(callback_can_change, expected_can_change);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  FakeCrostiniFeatures crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::ScopedCrosSettingsTestHelper scoped_settings_helper_{
      /* create_settings_service=*/false};

  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingChildUser) {
  AddChildUser();

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingManagedDisabledFeatureFlag) {
  SetFeatureFlag(false);

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingManagedDisallowedDevicePolicy) {
  SetFeatureFlag(true);
  SetDeviceToEnterpriseManaged();
  SetManagedUser(true);

  DisallowAdbSideloadingByDevicePolicy();

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingManagedUnaffiliatedUser) {
  SetFeatureFlag(true);
  SetDeviceToEnterpriseManaged();
  SetManagedUser(true);

  AllowAdbSideloadingByDevicePolicy();
  AddUserWithAffiliation(false);

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingManagedDisallowedUserPolicy) {
  SetFeatureFlag(true);
  SetDeviceToEnterpriseManaged();
  SetManagedUser(true);

  AllowAdbSideloadingByDevicePolicy();
  AddUserWithAffiliation(true);
  DisallowAdbSideloadingByUserPolicy();

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingManagedAllowedUserPolicy) {
  SetFeatureFlag(true);
  SetDeviceToEnterpriseManaged();
  SetManagedUser(true);

  AllowAdbSideloadingByDevicePolicy();
  AddUserWithAffiliation(true);
  AllowAdbSideloadingByUserPolicy();

  AssertCanChangeAdbSideloading(true);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingOwnerProfile) {
  SetDeviceToConsumerOwned();
  SetManagedUser(false);
  AddOwnerUser();

  AssertCanChangeAdbSideloading(true);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingOwnerProfileManagedUserDisallowed) {
  SetFeatureFlag(true);
  SetDeviceToConsumerOwned();
  SetManagedUser(true);
  AddOwnerUser();

  DisallowAdbSideloadingByUserPolicy();

  AssertCanChangeAdbSideloading(false);
}

TEST_F(CrostiniFeaturesAdbSideloadingTest,
       TestCanChangeAdbSideloadingOwnerProfileManagedUserAllowed) {
  SetFeatureFlag(true);
  SetDeviceToConsumerOwned();
  SetManagedUser(true);
  AddOwnerUser();

  AllowAdbSideloadingByUserPolicy();

  AssertCanChangeAdbSideloading(true);
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
