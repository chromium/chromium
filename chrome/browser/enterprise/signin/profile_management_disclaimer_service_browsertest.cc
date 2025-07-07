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

class ProfileManagementDisclaimerServiceStartupBrowserTest
    : public SigninBrowserTestBase {
 public:
  ProfileManagementDisclaimerServiceStartupBrowserTest()
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
    mutator.set_is_subject_to_enterprise_policies(hosted_domain !=
                                                  kNoHostedDomainFound);

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
      switches::kEnforceManagementDisclaimerAtStartup};
};

IN_PROC_BROWSER_TEST_F(ProfileManagementDisclaimerServiceStartupBrowserTest,
                       ShowsManagementDisclaimerOnBrowserFocused) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies());

  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate("bob@example.com",
                                                    "example.com");

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .IsManaged(),
            signin::Tribool::kTrue);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  // Still signed in while the dialog is shown
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(disclaimer_service->GetAccountBeingConsideredForManagementIfAny(),
            primary_account_info.account_id);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  auto* signin_view_controller =
      browser()->GetFeatures().signin_view_controller();
  ASSERT_TRUE(signin_view_controller->ShowsModalDialog());

  // Dismiss the dialog without any user choice.
  signin_view_controller->CloseModalSignin();
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // The account remains signed in the content area.
  ASSERT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
}

IN_PROC_BROWSER_TEST_F(ProfileManagementDisclaimerServiceStartupBrowserTest,
                       ConvertsToManagedProfile) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies());
  disclaimer_service->SetUserChoiceForTesting(
      signin::SigninChoice::SIGNIN_CHOICE_CONTINUE);

  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate("bob@example.com",
                                                    "example.com");

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .IsManaged(),
            signin::Tribool::kTrue);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  // Still signed in while the dialog is shown
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
                  .empty());
  ASSERT_TRUE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  ASSERT_FALSE(
      browser()->GetFeatures().signin_view_controller()->ShowsModalDialog());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementDisclaimerServiceStartupBrowserTest,
                       CreateNewManagedProfile) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies());
  disclaimer_service->SetUserChoiceForTesting(
      signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE);

  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate("bob@example.com",
                                                    "example.com");

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .IsManaged(),
            signin::Tribool::kTrue);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  base::test::TestFuture<Profile*, bool> future;
  disclaimer_service->EnsureManagedProfileForAccount(
      primary_account_info.account_id,
      signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup,
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  Profile* new_profile = future.Get<Profile*>();

  ASSERT_TRUE(new_profile);
  ASSERT_TRUE(disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
                  .empty());
  ASSERT_FALSE(
      browser()->GetFeatures().signin_view_controller()->ShowsModalDialog());

  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));

  auto* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  ASSERT_TRUE(enterprise_util::UserAcceptedAccountManagement(new_profile));
  ASSERT_TRUE(
      new_identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      primary_account_info.account_id));

  auto* new_disclaimer_service =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(new_profile);
  ASSERT_TRUE(
      new_disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
          .empty());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementDisclaimerServiceStartupBrowserTest,
                       CancelsWithoutPolicies) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies());
  disclaimer_service->SetUserChoiceForTesting(
      signin::SigninChoice::SIGNIN_CHOICE_CANCEL);

  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate("bob@example.com",
                                                    "example.com");

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .IsManaged(),
            signin::Tribool::kTrue);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  // Still signed in while the dialog is shown
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
                  .empty());
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  ASSERT_FALSE(
      browser()->GetFeatures().signin_view_controller()->ShowsModalDialog());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementDisclaimerServiceStartupBrowserTest,
                       CancelsWithProfileSeparationEnforced) {
  auto* disclaimer_service = GetDisclaimerService();

  disclaimer_service->SetProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::ENFORCED, std::nullopt));
  disclaimer_service->SetUserChoiceForTesting(
      signin::SigninChoice::SIGNIN_CHOICE_CANCEL);

  AccountInfo primary_account_info =
      MakeValidPrimaryAccountInfoAvailableAndUpdate("bob@example.com",
                                                    "example.com");

  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .IsManaged(),
            signin::Tribool::kTrue);
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));

  // Create a new browser to trigger the profile management disclaimer.
  ReplaceCurrentBrowserWithNewOne();

  // Still signed in while the dialog is shown
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(disclaimer_service->GetAccountBeingConsideredForManagementIfAny()
                  .empty());
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  ASSERT_FALSE(
      browser()->GetFeatures().signin_view_controller()->ShowsModalDialog());
}
