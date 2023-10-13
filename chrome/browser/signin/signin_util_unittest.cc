// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/test/browser_task_environment.h"

using signin_util::ProfileSeparationPolicyState;
using signin_util::ProfileSeparationPolicyStateSet;

namespace {

const char kLegacyPolicyEmpty[] = "";
const char kLegacyPolicyNone[] = "none";
const char kLegacyPolicyPrimaryAccount[] = "primary_account";
const char kLegacyPolicyPrimaryAccountStrict[] = "primary_account_strict";
const char kLegacyPolicyPrimaryAccountStrictKeepExistingData[] =
    "primary_account_strict_keep_existing_data";
const char kLegacyPolicyPrimaryAccountKeepExistingData[] =
    "primary_account_keep_existing_data";

}  // namespace

class SigninUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    signin_util::ResetForceSigninForTesting();
  }

  void TearDown() override {
    signin_util::ResetForceSigninForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  bool SeparationEnforcedByExistingProfileExpected(
      const std::string& local_policy) {
    return enforced_by_existing_profile.find(local_policy) !=
           enforced_by_existing_profile.end();
  }
  bool SeparationEnforcedByInterceptedAccountExpected(
      const std::string& intercepted_policy) {
    return enforced_by_intercepted_account.find(intercepted_policy) !=
           enforced_by_intercepted_account.end();
  }
  bool KeepBrowsingDataExpected(const std::string& local_policy,
                                const std::string& intercepted_policy) {
    return keeps_browsing_data.find(local_policy) !=
               keeps_browsing_data.end() &&
           keeps_browsing_data.find(intercepted_policy) !=
               keeps_browsing_data.end();
  }
  bool SeparationEnforcedOnMachineLevelExpected(
      const std::string& local_policy) {
    return enforced_on_machine_level.find(local_policy) !=
           enforced_on_machine_level.end();
  }

 protected:
  std::array<std::string, 6> all_policies{
      kLegacyPolicyEmpty,
      kLegacyPolicyNone,
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };

  base::flat_set<std::string> enforced_by_existing_profile{
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData};

  base::flat_set<std::string> enforced_by_intercepted_account{
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };

  base::flat_set<std::string> keeps_browsing_data{
      kLegacyPolicyEmpty,
      kLegacyPolicyNone,
      kLegacyPolicyPrimaryAccountKeepExistingData,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
  };

  base::flat_set<std::string> enforced_on_machine_level{
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };
};

TEST_F(SigninUtilTest, GetForceSigninPolicy) {
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());

  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               true);
  signin_util::ResetForceSigninForTesting();
  EXPECT_TRUE(signin_util::IsForceSigninEnabled());
  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               false);
  signin_util::ResetForceSigninForTesting();
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(SigninUtilTest, IsProfileSeparationEnforcedByProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile.get(), /*intercepted_account_email=*/std::string()),
              SeparationEnforcedByExistingProfileExpected(local_policy));
  }

  // Test profile set a machine level.
  profile.get()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);

  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile.get(), /*intercepted_account_email=*/std::string()),
              SeparationEnforcedOnMachineLevelExpected(local_policy));
  }
}

TEST_F(SigninUtilTest, IsProfileSeparationEnforcedByPolicies) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& intercepted_policy : all_policies) {
    EXPECT_EQ(
        signin_util::IsProfileSeparationEnforcedByPolicies(
            policy::ProfileSeparationPolicies(intercepted_policy)),
        SeparationEnforcedByInterceptedAccountExpected(intercepted_policy));
  }
}

TEST_F(
    SigninUtilTest,
    ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfileLegacy) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }

    for (const auto& intercepted_policy : all_policies) {
      EXPECT_EQ(
          signin_util::
              ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                  profile.get(),
                  policy::ProfileSeparationPolicies(intercepted_policy)),
          KeepBrowsingDataExpected(local_policy, intercepted_policy));
    }
  }
}

TEST_F(SigninUtilTest, IsSecondaryAccountAllowed) {
  const std::string consumer_email = "bob@gmail.com";
  const std::string enterprise_email = "bob@example.com";
  const std::string other_enterprise_email = "bob@bob.com";
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), consumer_email));
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), enterprise_email));
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), other_enterprise_email));

  {
    profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                   base::Value::List());

    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
  {
    base::Value::List profile_separation_exception_list;
    profile_separation_exception_list.Append(base::Value("bob.com"));
    profile()->GetPrefs()->SetList(
        prefs::kProfileSeparationDomainExceptionList,
        std::move(profile_separation_exception_list));

    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
  {
    base::Value::List profile_separation_exception_list;
    profile_separation_exception_list.Append(base::Value("bob.com"));
    profile_separation_exception_list.Append(base::Value("gmail.com"));
    profile()->GetPrefs()->SetList(
        prefs::kProfileSeparationDomainExceptionList,
        std::move(profile_separation_exception_list));

    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
}

TEST_F(SigninUtilTest,
       IsProfileSeparationEnforcedByProfileSecondaryAccountNotAllowed) {
  const std::string consumer_email = "bob@gmail.com";
  const std::string enterprise_email = "bob@example.com";
  const std::string other_enterprise_email = "bob@bob.com";

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(profile(),
                                                                consumer_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), other_enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
  }

  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 base::Value::List());

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), consumer_email))
        << policy;
    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), enterprise_email))
        << policy;
    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), other_enterprise_email))
        << policy;
  }

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("example.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), consumer_email))
        << policy;

    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), other_enterprise_email))
        << policy;
  }
}

TEST_F(SigninUtilTest, IsProfileSeparationEnforced) {
  EXPECT_FALSE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::SUGGESTED, absl::nullopt)));

  EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::ENFORCED, absl::nullopt)));

  EXPECT_FALSE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::DISABLED, absl::nullopt)));
}

TEST_F(SigninUtilTest,
       ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile) {
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                       local_policy);
    }

    EXPECT_EQ(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   USER_OPT_IN)),
        KeepBrowsingDataExpected(local_policy, std::string()))
        << local_policy;

    EXPECT_EQ(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   USER_OPT_OUT)),
        KeepBrowsingDataExpected(local_policy, std::string()))
        << local_policy;

    EXPECT_FALSE(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   ALWAYS_SEPARATE)))
        << local_policy;
  }
}

#endif
