// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"

#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

const ProfileAttributesEntry* GetExistingProfileEntryOtherThanSourceProfile(
    Profile* source_profile,
    const AccountInfo& account_info) {
  CHECK(source_profile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    CHECK_IS_TEST();
    return nullptr;
  }
  std::vector<ProfileAttributesEntry*> attributes =
      profile_manager->GetProfileAttributesStorage().GetAllProfilesAttributes();

  // Check if there is already an existing profile with this account.
  base::FilePath profile_path = source_profile->GetPath();
  auto it = std::find_if(
      attributes.begin(), attributes.end(),
      [&account_info, &profile_path](const ProfileAttributesEntry* entry) {
        return entry->GetPath() != profile_path &&
               account_info.gaia == entry->GetGAIAId();
      });
  return it == attributes.end() ? nullptr : *it;
}

}  // namespace

ManagedProfileCreationController::ManagedProfileCreationController(
    Profile* source_profile,
    const AccountInfo& account_info,
    signin_metrics::AccessPoint access_point,
    ManagedProfileCreationControllerCallback callback)
    : source_profile_(source_profile),
      account_info_(account_info),
      access_point_(access_point),
      callback_(std::move(callback)) {
  CHECK(source_profile_);
  CHECK(!account_info.IsEmpty());
  CHECK_EQ(account_info.IsManaged(), signin::Tribool::kTrue);
  source_profile_observation_.Observe(source_profile_);
}

ManagedProfileCreationController::~ManagedProfileCreationController() = default;

// static
std::unique_ptr<ManagedProfileCreationController>
ManagedProfileCreationController::CreateManagedProfile(
    Profile* source_profile,
    const AccountInfo& account_info,
    signin_metrics::AccessPoint access_point,
    ManagedProfileCreationControllerCallback callback) {
  // TODO(crbug.com/424782757): Log the errors in the callback.
  std::unique_ptr<ManagedProfileCreationController> controller;
  controller.reset(new ManagedProfileCreationController(
      source_profile, account_info, access_point, std::move(callback)));
  controller->FetchProfileSeparationPolicies();
  return controller;
}
// static
std::unique_ptr<ManagedProfileCreationController>
ManagedProfileCreationController::CreateManagedProfileForTesting(
    Profile* source_profile,
    const AccountInfo& account_info,
    signin_metrics::AccessPoint access_point,
    ManagedProfileCreationControllerCallback callback,
    std::optional<policy::ProfileSeparationPolicies>
        profile_separation_policies,
    std::optional<signin::SigninChoice> user_choice) {
  CHECK_IS_TEST();
  std::unique_ptr<ManagedProfileCreationController> controller;
  controller.reset(new ManagedProfileCreationController(
      source_profile, account_info, access_point, std::move(callback)));
  controller->profile_separation_policies_for_testing_ =
      std::move(profile_separation_policies);
  controller->user_choice_for_testing_ = std::move(user_choice);
  controller->FetchProfileSeparationPolicies();
  return controller;
}

void ManagedProfileCreationController::OnProfileWillBeDestroyed(
    Profile* profile) {
  CHECK(profile == source_profile_ || profile == final_profile_);

  if (profile == source_profile_) {
    source_profile_ = nullptr;
    source_profile_observation_.Reset();
  } else if (profile == final_profile_) {
    final_profile_ = nullptr;
    final_profile_observation_.Reset();
  }
  policy_fetch_timeout_.Stop();
  account_level_signin_restriction_policy_fetcher_.reset();
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        base::unexpected(
            profile == source_profile_
                ? ManagedProfileCreationFailureReason::kSourceProfileDeleted
                : ManagedProfileCreationFailureReason::kNewProfileWasDeleted),
        profile_creation_required_by_policy_);
  }
}

void ManagedProfileCreationController::FetchProfileSeparationPolicies() {
  // We should not fetch the policies twice.
  CHECK(!policies_received_);
  CHECK(!account_level_signin_restriction_policy_fetcher_);

  auto policy_fetch_callback = base::BindOnce(
      &ManagedProfileCreationController::OnProfileSeparationPoliciesReceived,
      weak_ptr_factory_.GetWeakPtr());

  if (profile_separation_policies_for_testing_.has_value()) {
    CHECK_IS_TEST();
    policy::ProfileSeparationPolicies profile_separation_policies =
        std::exchange(profile_separation_policies_for_testing_, std::nullopt)
            .value();
    std::move(policy_fetch_callback)
        .Run(std::move(profile_separation_policies));
    return;
  }

  // If we cannot make network calls, we will not be able to fetch the
  // account level policies.
  if (!g_browser_process->system_network_context_manager()) {
    std::move(policy_fetch_callback).Run(policy::ProfileSeparationPolicies());
    return;
  }

  CHECK(source_profile_);
  account_level_signin_restriction_policy_fetcher_ =
      std::make_unique<policy::UserCloudSigninRestrictionPolicyFetcher>(
          g_browser_process->browser_policy_connector(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory());
  account_level_signin_restriction_policy_fetcher_
      ->GetManagedAccountsSigninRestriction(
          GetIdentityManager(), account_info_.account_id,
          std::move(policy_fetch_callback),
          policy::utils::IsPolicyTestingEnabled(source_profile_->GetPrefs(),
                                                chrome::GetChannel())
              ? source_profile_->GetPrefs()
                    ->GetDefaultPrefValue(
                        prefs::kUserCloudSigninPolicyResponseFromPolicyTestPage)
                    ->GetString()
              : std::string());

  policy_fetch_timeout_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(&ManagedProfileCreationController::
                         OnProfileSeparationPoliciesReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     policy::ProfileSeparationPolicies()));
}

void ManagedProfileCreationController::OnProfileSeparationPoliciesReceived(
    policy::ProfileSeparationPolicies policies) {
  policy_fetch_timeout_.Stop();
  // If the profile was deleted in the meantime, we should not proceed.
  if (!source_profile_) {
    // `callback_` will be called with nullptr in OnProfileWillBeDestroyed() so
    // we are not calling it here.
    return;
  }
  policies_received_ = true;
  profile_creation_required_by_policy_ =
      signin_util::IsProfileSeparationEnforcedByPolicies(policies);
  allows_converting_profile_to_managed_ = signin_util::
      ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
          source_profile_, policies);

  // The fetcher must be deleted after `policies` have been used to avoid a use
  // after free.
  account_level_signin_restriction_policy_fetcher_.reset();

  // If the user is not allowed to sign in, we should not show the disclaimer.
  if (!source_profile_->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    // If the profile creation is required by policy, we should sign the user
    // out since they cannot sign in to Chrome.
    if (profile_creation_required_by_policy_) {
      Signout();
    } else {
      std::move(callback_).Run(base::ok(nullptr),
                               profile_creation_required_by_policy_);
    }
    return;
  }
  ShowManagementDisclaimer();
}

void ManagedProfileCreationController::ShowManagementDisclaimer() {
  CHECK(policies_received_);
  CHECK(source_profile_);
  Browser* browser = chrome::FindLastActiveWithProfile(source_profile_);
  bool has_browser_with_tab =
      browser &&
      browser->SupportsWindowFeature(Browser::WindowFeature::kFeatureTabStrip);

  if (user_choice_for_testing_.has_value()) {
    CHECK_IS_TEST();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ManagedProfileCreationController::OnManagementDisclaimerResult,
            weak_ptr_factory_.GetWeakPtr(), *user_choice_for_testing_));
    return;
  }

  if (!has_browser_with_tab) {
    // Posting the task here so that all code paths are asynchronous.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback_),
            base::unexpected(
                ManagedProfileCreationFailureReason::kNoActiveBrowser),
            profile_creation_required_by_policy_));
    return;
  }

  auto* switch_to_entry = GetExistingProfileEntryOtherThanSourceProfile(
      source_profile_, account_info_);
  bool managed_profile_already_exists =
      switch_to_entry && switch_to_entry->UserAcceptedAccountManagement();

  bool user_already_signed_in =
      GetIdentityManager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin) == account_info_.account_id;

  auto dialog_params =
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info_,
          /*is_OIDC_account=*/false,
          /*user_already_signed_in=*/user_already_signed_in,
          /*profile_creation_required_by_policy*/
          profile_creation_required_by_policy_,
          /*show_link_data_option=*/!managed_profile_already_exists &&
              allows_converting_profile_to_managed_,
          /*process_user_choice_callback=*/
          base::BindOnce(
              &ManagedProfileCreationController::OnManagementDisclaimerResult,
              weak_ptr_factory_.GetWeakPtr()),
          /*done_callback=*/
          base::BindOnce(
              &SigninViewController::CloseModalSignin,
              browser->GetFeatures().signin_view_controller()->AsWeakPtr()));
  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(std::move(dialog_params));
}

void ManagedProfileCreationController::OnManagementDisclaimerResult(
    signin::SigninChoice choice) {
  // If the profile was deleted in the meantime, we should not proceed.
  if (!source_profile_) {
    // `callback_` will be called with nullptr in OnProfileWillBeDestroyed() so
    // we are not calling it here.
    return;
  }
  switch (choice) {
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      MoveAccountIntoNewProfile();
      break;
    case signin::SIGNIN_CHOICE_CANCEL:
      Signout();
      break;
    case signin::SIGNIN_CHOICE_CONTINUE:
      ConvertSourceProfileIntoManagedProfile();
      break;
    case signin::SIGNIN_CHOICE_SIZE:
    default:
      NOTREACHED();
  }
}

void ManagedProfileCreationController::
    ConvertSourceProfileIntoManagedProfile() {
  CHECK(source_profile_);
  enterprise_util::SetUserAcceptedAccountManagement(source_profile_, true);
  auto existing_primary_account_id =
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  CHECK(existing_primary_account_id.empty() ||
        existing_primary_account_id == account_info_.account_id);

  if (existing_primary_account_id.empty()) {
    auto* primary_account_mutator =
        GetIdentityManager()->GetPrimaryAccountMutator();
    primary_account_mutator->SetPrimaryAccount(
        account_info_.account_id, signin::ConsentLevel::kSignin, access_point_);
  }
  std::move(callback_).Run(base::ok(source_profile_),
                           profile_creation_required_by_policy_);
}

void ManagedProfileCreationController::Signout() {
  CHECK(source_profile_);
  auto* primary_account_mutator =
      GetIdentityManager()->GetPrimaryAccountMutator();
  if (account_info_.account_id == GetIdentityManager()->GetPrimaryAccountId(
                                      signin::ConsentLevel::kSignin)) {
    primary_account_mutator->RemovePrimaryAccountButKeepTokens(
        signin_metrics::ProfileSignout::
            kUserDeclinedEnterpriseManagementDisclaimer);
  }
  if (profile_creation_required_by_policy_) {
    auto* accounts_mutator = GetIdentityManager()->GetAccountsMutator();
    accounts_mutator->RemoveAccount(
        account_info_.account_id,
        signin_metrics::SourceForRefreshTokenOperation::
            kEnterpriseForcedProfileCreation_UserDecline);
  }
  std::move(callback_).Run(base::ok(nullptr),
                           profile_creation_required_by_policy_);
}

void ManagedProfileCreationController::MoveAccountIntoNewProfile() {
  CHECK(source_profile_);
  auto* switch_to_entry = GetExistingProfileEntryOtherThanSourceProfile(
      source_profile_, account_info_);
  bool managed_profile_already_exists =
      switch_to_entry && switch_to_entry->UserAcceptedAccountManagement();

  std::u16string profile_name =
      profiles::GetDefaultNameForNewSignedInProfile(account_info_);

  CHECK(!profile_creator_);
  if (managed_profile_already_exists) {
    profile_creator_ = std::make_unique<DiceSignedInProfileCreator>(
        source_profile_, account_info_.account_id, switch_to_entry->GetPath(),
        base::BindOnce(
            &ManagedProfileCreationController::OnNewSignedInProfileCreated,
            weak_ptr_factory_.GetWeakPtr(),
            /*is_new_profile=*/false));

    return;
  }
  profile_creator_ = std::make_unique<DiceSignedInProfileCreator>(
      source_profile_, account_info_.account_id, profile_name,
      profiles::GetPlaceholderAvatarIndex(),
      base::BindOnce(
          &ManagedProfileCreationController::OnNewSignedInProfileCreated,
          weak_ptr_factory_.GetWeakPtr(),
          /*is_new_profile=*/true));
}

void ManagedProfileCreationController::OnNewSignedInProfileCreated(
    bool is_new_profile,
    Profile* new_profile) {
  CHECK(profile_creator_);
  profile_creator_.reset();

  if (!new_profile) {
    std::move(callback_).Run(
        base::unexpected(
            ManagedProfileCreationFailureReason::kProfileCreationFailed),
        profile_creation_required_by_policy_);
    return;
  }

  const CoreAccountId new_profile_primary_account_id =
      IdentityManagerFactory::GetForProfile(new_profile)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id;

  // If the account failed to move into the new profile, we should not proceed.
  if (new_profile_primary_account_id.empty()) {
    // If the profile creation is required by policy, we should sign the user
    // out since they failed to sign in to Chrome.
    if (profile_creation_required_by_policy_) {
      Signout();
    } else {
      std::move(callback_).Run(
          base::unexpected(
              ManagedProfileCreationFailureReason::kPrimaryAccountNotSet),
          profile_creation_required_by_policy_);
    }
    return;
  }

  CHECK_EQ(new_profile_primary_account_id, account_info_.account_id);

  final_profile_ = new_profile;
  final_profile_observation_.Observe(final_profile_);

  ProfileAttributesEntry* entry = nullptr;
  if (source_profile_) {
    entry = g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(source_profile_->GetPath());
  }
  // Apply the new color to the profile.
  ThemeServiceFactory::GetForProfile(new_profile)
      ->SetUserColorAndBrowserColorVariant(
          GenerateNewProfileColor(entry).color,
          ui::mojom::BrowserColorVariant::kTonalSpot);

  if (source_profile_) {
    // The new profile inherits the default search provider and the search
    // engine choice timestamp from the previous profile.
    SearchEngineChoiceDialogService::UpdateProfileFromChoiceData(
        *new_profile, SearchEngineChoiceDialogService::GetChoiceDataFromProfile(
                          *source_profile_));
  }

  enterprise_util::SetUserAcceptedAccountManagement(new_profile, true);
  CHECK(enterprise_util::UserAcceptedAccountManagement(new_profile));

  if (skip_browser_startup_for_testing_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), base::ok(new_profile),
                                  profile_creation_required_by_policy_));
    return;
  }

  startup_helper_ = std::make_unique<DiceInterceptedSessionStartupHelper>(
      new_profile, is_new_profile, account_info_.account_id, nullptr);
  startup_helper_->Startup(
      base::BindOnce(&ManagedProfileCreationController::OnNewBrowserCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ManagedProfileCreationController::OnNewBrowserCreated() {
  // Profile was deleted in the meantime.
  if (!final_profile_) {
    return;
  }
  CHECK(enterprise_util::UserAcceptedAccountManagement(final_profile_));
  std::move(callback_).Run(final_profile_,
                           profile_creation_required_by_policy_);
}

signin::IdentityManager*
ManagedProfileCreationController::GetIdentityManager() {
  CHECK(source_profile_);
  return IdentityManagerFactory::GetForProfile(source_profile_);
}
