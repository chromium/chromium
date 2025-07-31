// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::constants::kNoHostedDomainFound;

namespace {
signin::IdentityManager* GetIdentityManager(Profile* profile) {
  return IdentityManagerFactory::GetForProfile(profile);
}
}  // namespace

enum class ManagedProfileCreationResult {
  kNull,
  kExistingProfile,
  kNewProfile
};

// The test is going to be defined by these params that should be
// self-explanatory.
struct ManagementDisclaimerTestParam {
  std::string test_name;
  // Preconditions:
  std::optional<signin::SigninChoice> user_choice = std::nullopt;
  policy::ProfileSeparationPolicies policies;
  bool is_managed;

  // Expectations:
  ManagedProfileCreationResult expected_profile_result;
  bool expected_management_accepted;
  bool expected_primary_account;
  bool expected_refresh_token;
};

const ManagementDisclaimerTestParam kManagementDisclaimerTestParams[] = {
    // - Not managed
    {
        .test_name = "NotManaged_NoPolicies_NewProfile",
        .user_choice = std::nullopt,
        .policies = policy::ProfileSeparationPolicies(),
        .is_managed = false,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_management_accepted = false,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Managed
    // - No policies
    // - User choice: New profile
    {
        .test_name = "Managed_NoPolicies_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Managed
    // - Profile creation is enforced by policy
    // - User choice: New profile
    {
        .test_name = "Managed_EnforcedByPolicy_NewProfile",
        .user_choice = signin::SIGNIN_CHOICE_NEW_PROFILE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNewProfile,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Managed
    // - No policies
    // - User choice: Convert to managed profile
    {
        .test_name = "Managed_NoPolicies_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(),
        .is_managed = true,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Managed
    // - Profile creation is enforced by policy
    // - User choice: Convert to managed profile
    {
        .test_name = "Managed_EnforcedByPolicy_Continue",
        .user_choice = signin::SIGNIN_CHOICE_CONTINUE,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_managed = true,

        .expected_profile_result =
            ManagedProfileCreationResult::kExistingProfile,
        .expected_management_accepted = true,
        .expected_primary_account = true,
        .expected_refresh_token = true,
    },
    // - Managed
    // - No policies
    // - User choice: Cancel
    {
        .test_name = "Managed_NoPolicies_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = true,
    },
    // - Managed
    // - Profile creation is enforced by policy
    // - User choice: Cancel
    {
        .test_name = "Managed_EnforcedByPolicy_Cancel",
        .user_choice = signin::SIGNIN_CHOICE_CANCEL,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = false,
    },
    // - Managed
    // - No policies
    // - No User choice
    {
        .test_name = "Managed_NoPolicies_Dismiss",
        .user_choice = std::nullopt,
        .policies = policy::ProfileSeparationPolicies(),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = true,
    },
    // - Managed
    // - Profile creation is enforced by policy
    // - No User choice
    {
        .test_name = "Managed_EnforcedByPolicy_Dismiss",
        .user_choice = std::nullopt,
        .policies = policy::ProfileSeparationPolicies(
            /*profile_separation_settings=*/policy::ProfileSeparationSettings::
                ENFORCED,
            /*profile_separation_data_migration_settings=*/std::nullopt),
        .is_managed = true,

        .expected_profile_result = ManagedProfileCreationResult::kNull,
        .expected_management_accepted = false,
        .expected_primary_account = false,
        .expected_refresh_token = false,
    },
};

class ProfileManagementDisclaimerServiceBrowserFocusBrowserTest
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<ManagementDisclaimerTestParam> {
 public:
  ProfileManagementDisclaimerServiceBrowserFocusBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  AccountInfo MakeValidPrimaryAccountInfoAvailableAndUpdate(
      const std::string& email,
      const std::string& hosted_domain) {
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = hosted_domain;
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    bool is_managed = hosted_domain != kNoHostedDomainFound;
    mutator.set_is_subject_to_account_level_enterprise_policies(is_managed);
    mutator.set_is_subject_to_enterprise_features(is_managed);

    DCHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  ProfileManagementDisclaimerService* GetDisclaimerService() {
    return ProfileManagementDisclaimerServiceFactory::GetForProfile(
        GetProfile());
  }

  void ReplaceCurrentBrowserWithNewOne() {
    Browser* new_browser = CreateBrowser(browser()->profile());
    CloseBrowserSynchronously(browser());
    SelectFirstBrowser();
    ASSERT_EQ(browser(), new_browser);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnforceManagementDisclaimer};
};

IN_PROC_BROWSER_TEST_P(
    ProfileManagementDisclaimerServiceBrowserFocusBrowserTest,
    Test) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      GetParam().policies);
  if (GetParam().user_choice.has_value()) {
    disclaimer_service->SetUserChoiceForTesting(GetParam().user_choice.value());
  }

  auto resetter = disclaimer_service->DisableManagementDisclaimerUntilReset();
  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate(
          "bob@example.com",
          GetParam().is_managed ? "example.com" : kNoHostedDomainFound);
  base::RunLoop().RunUntilIdle();
  std::move(resetter).RunAndReset();

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .CanApplyAccountLevelEnterprisePolicies(),
            signin::TriboolFromBool(GetParam().is_managed));
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  Profile* new_profile = nullptr;

  if (GetParam().user_choice.has_value()) {
    base::test::TestFuture<Profile*, bool> future;
    disclaimer_service->EnsureManagedProfileForAccount(
        primary_account_info.account_id,
        signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup,
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    new_profile = future.Get<Profile*>();
  }

  auto* signin_view_controller =
      browser()->GetFeatures().signin_view_controller();
  if (!GetParam().is_managed) {
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(signin_view_controller->ShowsModalDialog());
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
    return;
  }

  if (!GetParam().user_choice.has_value()) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(signin_view_controller->ShowsModalDialog());

    // Dismiss the dialog without any user choice.
    signin_view_controller->CloseModalSignin();
  }

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

  EXPECT_EQ(enterprise_util::UserAcceptedAccountManagement(verify_profile),
            GetParam().expected_management_accepted);
  EXPECT_EQ(GetIdentityManager(verify_profile)
                ->HasAccountWithRefreshToken(primary_account_info.account_id),
            GetParam().expected_refresh_token);

  if (verify_profile != GetProfile()) {
    EXPECT_EQ(GetIdentityManager(verify_profile)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin),
              GetParam().expected_primary_account);
  }

  // Also check the source profile if a new one was created.
  if (GetParam().expected_profile_result ==
      ManagedProfileCreationResult::kNewProfile) {
    EXPECT_FALSE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
    EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
        primary_account_info.account_id));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileManagementDisclaimerServiceBrowserFocusBrowserTest,
    testing::ValuesIn(kManagementDisclaimerTestParams),
    [](const auto& info) { return info.param.test_name; });

class ProfileManagementDisclaimerServiceSigninBrowserTest
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<ManagementDisclaimerTestParam> {
 public:
  ProfileManagementDisclaimerServiceSigninBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  AccountInfo MakeValidPrimaryAccountInfoAvailableAndUpdate(
      const std::string& email,
      const std::string& hosted_domain) {
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    return MakeValidAccountInfoForAccount(std::move(account_info),
                                          hosted_domain);
  }

  AccountInfo MakeValidAccountInfoForAccount(AccountInfo&& account_info,
                                             const std::string& hosted_domain) {
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = hosted_domain;
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    bool is_managed = hosted_domain != kNoHostedDomainFound;
    mutator.set_is_subject_to_enterprise_features(is_managed);
    mutator.set_is_subject_to_account_level_enterprise_policies(is_managed);

    DCHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  ProfileManagementDisclaimerService* GetDisclaimerService() {
    return ProfileManagementDisclaimerServiceFactory::GetForProfile(
        GetProfile());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnforceManagementDisclaimer};
};

IN_PROC_BROWSER_TEST_P(ProfileManagementDisclaimerServiceSigninBrowserTest,
                       Test) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      GetParam().policies);
  if (GetParam().user_choice.has_value()) {
    disclaimer_service->SetUserChoiceForTesting(GetParam().user_choice.value());
  }

  // No disclaimer should be shown while the profile is not signed in.
  ASSERT_TRUE(disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
                  .empty());
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Set primary account with no extended info.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(disclaimer_service->GetAccountBeingConsideredForManagementIfAny(),
            primary_account_info.account_id);

  primary_account_info = MakeValidAccountInfoForAccount(
      std::move(primary_account_info),
      GetParam().is_managed ? "example.com" : kNoHostedDomainFound);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .CanApplyAccountLevelEnterprisePolicies(),
            signin::TriboolFromBool(GetParam().is_managed));
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  Profile* new_profile = nullptr;

  auto* signin_view_controller =
      browser()->GetFeatures().signin_view_controller();

  if (GetParam().user_choice.has_value()) {
    base::test::TestFuture<Profile*, bool> future;
    disclaimer_service->EnsureManagedProfileForAccount(
        primary_account_info.account_id,
        signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup,
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
    new_profile = future.Get<Profile*>();
  }

  if (!GetParam().is_managed) {
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(signin_view_controller->ShowsModalDialog());
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
    return;
  }

  if (!GetParam().user_choice.has_value()) {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(signin_view_controller->ShowsModalDialog());

    // Dismiss the dialog without any user choice.
    signin_view_controller->CloseModalSignin();
  }

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

  EXPECT_EQ(enterprise_util::UserAcceptedAccountManagement(verify_profile),
            GetParam().expected_management_accepted);
  EXPECT_EQ(GetIdentityManager(verify_profile)
                ->HasAccountWithRefreshToken(primary_account_info.account_id),
            GetParam().expected_refresh_token);

  if (verify_profile != GetProfile()) {
    EXPECT_EQ(GetIdentityManager(verify_profile)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin),
              GetParam().expected_primary_account);
  }

  // Also check the source profile if a new one was created.
  if (GetParam().expected_profile_result ==
      ManagedProfileCreationResult::kNewProfile) {
    EXPECT_FALSE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
    EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
        primary_account_info.account_id));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileManagementDisclaimerServiceSigninBrowserTest,
                         testing::ValuesIn(kManagementDisclaimerTestParams),
                         [](const auto& info) { return info.param.test_name; });
