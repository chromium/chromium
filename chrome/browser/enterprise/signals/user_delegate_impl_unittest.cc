// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_delegate_impl.h"

#include <set>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/fake_device_trust_connector_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_signals {

using DTCPolicyLevel = enterprise_connectors::DTCPolicyLevel;

namespace {

constexpr char kUserEmail[] = "someEmail@example.com";
constexpr char kOtherUserEmail[] = "someOtherUser@example.com";
constexpr char kOtherUserGaiaId[] = "some-other-user-gaia";

base::Value::List GetUrls() {
  base::Value::List trusted_urls;
  trusted_urls.Append("https://www.example.com");
  trusted_urls.Append("example2.example.com");
  return trusted_urls;
}

}  // namespace

class UserDelegateImplTest : public testing::Test {
 protected:
  void CreateDelegate(
      bool is_managed_user = true,
      std::optional<base::FilePath> profile_path = std::nullopt) {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(is_managed_user);

    if (profile_path) {
      builder.SetPath(profile_path.value());
    }

    testing_profile_ = builder.Build();

    fake_dt_connector_service_ = std::make_unique<
        enterprise_connectors::FakeDeviceTrustConnectorService>(
        testing_profile_->GetTestingPrefService());

    user_delegate_ = std::make_unique<UserDelegateImpl>(
        testing_profile_.get(), identity_test_env_.identity_manager(),
        fake_dt_connector_service_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<enterprise_connectors::FakeDeviceTrustConnectorService>
      fake_dt_connector_service_;

  std::unique_ptr<UserDelegateImpl> user_delegate_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the sign-in profile is considered as sign-in context.
TEST_F(UserDelegateImplTest, IsSigninContext_True) {
  CreateDelegate(/*is_managed_user=*/true,
                 base::FilePath(ash::kSigninBrowserContextBaseName));
  EXPECT_TRUE(user_delegate_->IsSigninContext());
}

// Tests that a regular profile is not considered as sign-in context.
TEST_F(UserDelegateImplTest, IsSigninContext_False) {
  CreateDelegate();
  EXPECT_FALSE(user_delegate_->IsSigninContext());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that IsManagedUser returns false when the user is not managed.
TEST_F(UserDelegateImplTest, IsManagedUser_False) {
  CreateDelegate(/*is_managed_user=*/false);
  EXPECT_FALSE(user_delegate_->IsManagedUser());
}

// Tests that IsManagedUser returns true when the user is managed.
TEST_F(UserDelegateImplTest, IsManagedUser_True) {
  CreateDelegate();
  EXPECT_TRUE(user_delegate_->IsManagedUser());
}

// Tests that IsSameUser returns false when the identity manager
// is a nullptr.
TEST_F(UserDelegateImplTest, IsSameUser_NullManager) {
  // Instantiate all of the dependencies and reset the delegate.
  CreateDelegate();
  user_delegate_ = std::make_unique<UserDelegateImpl>(
      testing_profile_.get(), nullptr, fake_dt_connector_service_.get());

  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);
  EXPECT_FALSE(user_delegate_->IsSameUser(account.gaia));
}

// Tests that IsSameUser returns false when given a different user.
TEST_F(UserDelegateImplTest, IsSameManagedUser_DifferentUser) {
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);
  auto other_account = identity_test_env_.MakeAccountAvailable(
      kOtherUserEmail, {.set_cookie = true, .gaia_id = kOtherUserGaiaId});

  CreateDelegate();
  EXPECT_FALSE(user_delegate_->IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns false when there is no primary user.
TEST_F(UserDelegateImplTest, IsSameUser_NoPrimaryUser) {
  auto other_account = identity_test_env_.MakeAccountAvailable(
      kOtherUserEmail, {.set_cookie = true, .gaia_id = kOtherUserGaiaId});

  CreateDelegate();
  EXPECT_FALSE(user_delegate_->IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns true when given the same user, and the
// user did not give Sync consent.
TEST_F(UserDelegateImplTest, IsSameUser_SameUser_Signin) {
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);

  CreateDelegate();
  EXPECT_TRUE(user_delegate_->IsSameUser(account.gaia));
}

// Tests that IsSameUser returns true when given the same user, and the
// user gave Sync consent.
TEST_F(UserDelegateImplTest, IsSameUser_SameUser_Sync) {
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSync);

  CreateDelegate();
  EXPECT_TRUE(user_delegate_->IsSameUser(account.gaia));
}

// Tests that GetPolicyScopesNeedingSignals returns an empty set when
// no policies are enabled.
TEST_F(UserDelegateImplTest, GetPolicyScopesNeedingSignals_Empty) {
  CreateDelegate();
  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>());
}

// Tests what GetPolicyScopesNeedingSignals returns when the policy is enabled
// at the user level.
TEST_F(UserDelegateImplTest, GetPolicyScopesNeedingSignals_User) {
  CreateDelegate();
  fake_dt_connector_service_->UpdateInlinePolicy(GetUrls(),
                                                 DTCPolicyLevel::kUser);

  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>({policy::POLICY_SCOPE_USER}));
}

// Tests what GetPolicyScopesNeedingSignals returns when the policy is enabled
// at the browser level.
TEST_F(UserDelegateImplTest, GetPolicyScopesNeedingSignals_Browser) {
  CreateDelegate();
  fake_dt_connector_service_->UpdateInlinePolicy(GetUrls(),
                                                 DTCPolicyLevel::kBrowser);
  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>({policy::POLICY_SCOPE_MACHINE}));
}

// Tests what GetPolicyScopesNeedingSignals returns when the policy is enabled
// at the both the user and browser levels.
TEST_F(UserDelegateImplTest, GetPolicyScopesNeedingSignals_UserAndBrowser) {
  CreateDelegate();
  fake_dt_connector_service_->UpdateInlinePolicy(GetUrls(),
                                                 DTCPolicyLevel::kBrowser);
  fake_dt_connector_service_->UpdateInlinePolicy(GetUrls(),
                                                 DTCPolicyLevel::kUser);

  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>(
                {policy::POLICY_SCOPE_MACHINE, policy::POLICY_SCOPE_USER}));
}

}  // namespace enterprise_signals
