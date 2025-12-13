// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

enum class ManagedProfileCreationResult {
  kNull,
  kExistingProfile,
  kNewProfile
};

// The test is going to be defined by these params that should be
// self-explanatory.
struct ManagedProfileCreationTestParam {
  std::string test_name;
  // Preconditions:
  signin::SigninChoice user_choice;
  policy::ProfileSeparationPolicies policies;
  bool is_primary_account;
  bool has_other_primary_account;

  // Expectations:
  ManagedProfileCreationResult expected_profile_result;
  bool expected_profile_creation_required_by_policy;
  bool expected_management_accepted;
  bool expected_primary_account;
  bool expected_refresh_token;
};

const ManagedProfileCreationTestParam kManagedProfileCreationTestParams[] = {
    /* NO PRIMARY ACCOUNT TEST CASES */
    // - No policies
    // - User choice: New profile
    {
        .test_name = "NoPrimaryAccount_NoPolicies_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: New profile
    {
        .test_name = "NoPrimaryAccount_EnforcedByPolicy_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - No policies
    // - User choice: Convert to managed profile
    {
        .test_name = "NoPrimaryAccount_NoPolicies_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: Convert to managed profile
    {
        .test_name = "NoPrimaryAccount_EnforcedByPolicy_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - No policies
    // - User choice: Cancel
    {
        .test_name = "NoPrimaryAccount_NoPolicies_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: Cancel
    {
        .test_name = "NoPrimaryAccount_EnforcedByPolicy_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = false,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = false,
    },
    /*SAME PRIMARY ACCOUNT TEST CASES*/
    // - No policies
    // - User choice: New profile
    {
        .test_name = "SamePrimaryAccount_NoPolicies_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: New profile
    {
        .test_name = "SamePrimaryAccount_EnforcedByPolicy_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - No policies
    // - User choice: Convert to managed profile
    {
        .test_name = "SamePrimaryAccount_NoPolicies_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: Convert to managed profile
    {
        .test_name = "SamePrimaryAccount_EnforcedByPolicy_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - No policies
    // - User choice: Cancel
    {
        .test_name = "SamePrimaryAccount_NoPolicies_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: Cancel
    {
        .test_name = "SamePrimaryAccount_EnforcedByPolicy_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = true,
        .has_other_primary_account = false,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = false,
    },
    /*OTHER PRIMARY ACCOUNT TEST CASES*/
    // - No policies
    // - User choice: New profile
    {
        .test_name = "OtherPrimaryAccount_NoPolicies_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = false,
        .has_other_primary_account = true,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: New profile
    {
        .test_name = "OtherPrimaryAccount_EnforcedByPolicy_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = false,
        .has_other_primary_account = true,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - No policies
    // - User choice: Cancel
    {
        .test_name = "OtherPrimaryAccount_NoPolicies_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(),
        .is_primary_account = false,
        .has_other_primary_account = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = false,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = true,
    },
    // - Profile creation is enforced by policy
    // - User choice: Cancel
    {
        .test_name = "OtherPrimaryAccount_EnforcedByPolicy_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_primary_account = false,
        .has_other_primary_account = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_profile_creation_required_by_policy = true,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = false,
    },
};

class ManagedProfileCreationBrowserTest
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<ManagedProfileCreationTestParam> {
 public:
  ManagedProfileCreationBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            GetProfile());
  }

  AccountInfo MakeValidAccountInfoAvailableAndUpdate(
      const std::string& email,
      const std::string& hosted_domain,
      bool primary_account = false) {
    std::optional<signin::ConsentLevel> consent_level;
    if (primary_account) {
      consent_level = signin::ConsentLevel::kSignin;
    }
    AccountInfo account_info = identity_test_env()->MakeAccountAvailable(
        email, signin::SimpleAccountAvailabilityOptions{
                   .primary_account_consent_level = consent_level});
    // Fill the account info, in particular for the hosted_domain field.
    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("fullname")
                       .SetGivenName("givenname")
                       .SetHostedDomain(hosted_domain)
                       .SetLocale("en")
                       .SetAvatarUrl("https://example.com")
                       .Build();

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_enterprise_features(!hosted_domain.empty());
    mutator.set_is_subject_to_account_level_enterprise_policies(
        !hosted_domain.empty());

    DCHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  signin::IdentityManager* GetIdentityManager(Profile* profile) {
    return IdentityManagerFactory::GetForProfile(profile);
  }

  signin::IdentityManager* GetIdentityManager() {
    return GetIdentityManager(GetProfile());
  }

 private:
  base::ScopedClosureRunner disclaimer_service_resetter_;
};

IN_PROC_BROWSER_TEST_P(ManagedProfileCreationBrowserTest, Test) {
  // Arrange:
  AccountInfo other_primary_account_info;
  if (GetParam().has_other_primary_account) {
    other_primary_account_info = MakeValidAccountInfoAvailableAndUpdate(
        "alice@example.com", "example.com",
        /*primary_account=*/true);
    // Make sure the we only set one primary account.
    ASSERT_FALSE(GetParam().is_primary_account);
  }
  auto account_info = MakeValidAccountInfoAvailableAndUpdate(
      "bob@example.com", "example.com", GetParam().is_primary_account);

  // Act:
  base::test::TestFuture<
      base::expected<Profile*, ManagedProfileCreationFailureReason>, bool>
      future;
  auto managed_profile_creation_controller =
      ManagedProfileCreationController::CreateManagedProfileForTesting(
          GetProfile(), account_info, signin_metrics::AccessPoint::kUnknown,
          future.GetCallback(), GetParam().policies, GetParam().user_choice);
  ASSERT_TRUE(future.Wait());
  Profile* new_profile =
      future
          .Get<base::expected<Profile*, ManagedProfileCreationFailureReason>>()
          .value_or(nullptr);
  bool profile_creation_required_by_policy = future.Get<bool>();

  // Verify:
  Profile* verify_profile = nullptr;
  switch (GetParam().expected_profile_result) {
    case ManagedProfileCreationResult::kNull:
      EXPECT_EQ(new_profile, nullptr);
      verify_profile = GetProfile();
      break;
    case ManagedProfileCreationResult::kExistingProfile:
      EXPECT_EQ(new_profile, GetProfile());
      verify_profile = GetProfile();
      break;
    case ManagedProfileCreationResult::kNewProfile:
      EXPECT_NE(new_profile, GetProfile());
      verify_profile = new_profile;
      break;
  }
  EXPECT_EQ(profile_creation_required_by_policy,
            GetParam().expected_profile_creation_required_by_policy);
  EXPECT_EQ(enterprise_util::UserAcceptedAccountManagement(verify_profile),
            GetParam().expected_management_accepted);
  EXPECT_EQ(GetIdentityManager(verify_profile)
                ->HasAccountWithRefreshToken(account_info.account_id),
            GetParam().expected_refresh_token);

  // The other primary account should not have been touched.
  if (GetParam().has_other_primary_account) {
    EXPECT_TRUE(GetIdentityManager(GetProfile())
                    ->GetPrimaryAccountId(signin::ConsentLevel::kSignin) ==
                other_primary_account_info.account_id);
    EXPECT_TRUE(GetIdentityManager(GetProfile())
                    ->HasAccountWithRefreshToken(
                        other_primary_account_info.account_id));
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  }
  if (verify_profile != GetProfile() || !GetParam().has_other_primary_account) {
    EXPECT_EQ(GetIdentityManager(verify_profile)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin),
              GetParam().expected_primary_account);
  }

  // Also check the source profile if a new one was created.
  if (GetParam().expected_profile_result ==
      ManagedProfileCreationResult::kNewProfile) {
    EXPECT_EQ(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin),
        GetParam().has_other_primary_account);
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
    EXPECT_FALSE(GetIdentityManager()->HasAccountWithRefreshToken(
        account_info.account_id));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ManagedProfileCreationBrowserTest,
                         testing::ValuesIn(kManagedProfileCreationTestParams),
                         [](const auto& info) { return info.param.test_name; });
