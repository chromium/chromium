// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/primary_account_policy_manager.h"

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/primary_account_policy_manager_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrimaryAccountPolicyManagerTest : public testing::Test {
 public:
  PrimaryAccountPolicyManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  ~PrimaryAccountPolicyManagerTest() override = default;

  void CreateTestingProfile() {
    DCHECK(!profile_);

    profile_ = profile_manager_.CreateTestingProfile(
        "primary_account_policy_manager_test_profile_path",
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  void DestroyProfile() {
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteTestingProfile(
        "primary_account_policy_manager_test_profile_path");
  }

  PrefService* GetLocalState() { return profile_manager_.local_state()->Get(); }

  TestingProfile* GetProfile() {
    DCHECK(profile_);
    return profile_;
  }

  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// All primary accounts are allowed on ChromeOS, so this the
// PrimaryAccountPolicyManagerTest does not clear the primary account on
// ChromeOS.
//
// TODO(msarda): Exclude |PrimaryAccountPolicyManager| from the ChromeOS
// build.
TEST_F(PrimaryAccountPolicyManagerTest,
       ClearPrimarySyncAccountWhenSigninNotAllowed) {
  CreateTestingProfile();
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}

TEST_F(PrimaryAccountPolicyManagerTest,
       ClearPrimarySyncAccountWhenPatternNotAllowed) {
  CreateTestingProfile();
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);
  GetLocalState()->SetString(prefs::kGoogleServicesUsernamePattern,
                             ".*@bar.com");

  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
