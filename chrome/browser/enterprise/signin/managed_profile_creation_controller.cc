// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_creation_controller.h"

#include "base/check_is_test.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
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
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
               account_info.GetGaiaId() == entry->GetGAIAId();
      });
  return it == attributes.end() ? nullptr : *it;
}

}  // namespace

ManagedProfileCreationController::ManagedProfileCreationController(
    Profile* source_profile,
    const AccountInfo& account_info,
    signin_metrics::AccessPoint access_point,
    ManagedProfileCreationControllerCallback callback,
    policy::ProfileSeparationPolicies profile_separation_policies)
    : source_profile_(source_profile),
      account_info_(account_info),
      access_point_(access_point),
      profile_separation_policies_(std::move(profile_separation_policies)),
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
    ManagedProfileCreationControllerCallback callback,
    policy::ProfileSeparationPolicies profile_separation_policies) {
  // TODO(crbug.com/424782757): Log the errors in the callback.
  std::unique_ptr<ManagedProfileCreationController> controller;
  controller.reset(new ManagedProfileCreationController(
      source_profile, account_info, access_point, std::move(callback),
      std::move(profile_separation_policies)));
  if (!controller->Init()) {
    return nullptr;
  }
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
      source_profile, account_info, access_point, std::move(callback),
      profile_separation_policies.value_or(
          policy::ProfileSeparationPolicies())));
  controller->user_choice_for_testing_ = user_choice;
  controller->skip_browser_startup_for_testing_ = true;
  if (!controller->Init()) {
    return nullptr;
  }
  return controller;
}

void ManagedProfileCreationController::OnProfileWillBeDestroyed(
    Profile* profile) {
  CHECK(profile == source_profile_ || profile == final_profile_);

  const bool is_source_profile = (profile == source_profile_);
  if (is_source_profile) {
    source_profile_ = nullptr;
    source_profile_observation_.Reset();
  } else {
    final_profile_ = nullptr;
    final_profile_observation_.Reset();
  }

  OnProfileCreationDone(base::unexpected(
      profile == source_profile_
          ? ManagedProfileCreationFailureReason::kSourceProfileDeleted
          : ManagedProfileCreationFailureReason::kNewProfileWasDeleted));
}

bool ManagedProfileCreationController::Init() {
  CHECK(source_profile_);

  profile_creation_required_by_policy_ =
      signin_util::IsProfileSeparationEnforcedByPolicies(
          profile_separation_policies_);
  allows_converting_profile_to_managed_ = signin_util::
      ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
          source_profile_, profile_separation_policies_);

  // If the user is not allowed to sign in, we should not show the disclaimer.
  SigninUIError can_offer_error = CanOfferSignin(
      source_profile_, account_info_.GetGaiaId(), account_info_.GetEmail(),
      /*allow_account_from_other_profile=*/true,
      /*ignore_reauth_error=*/true);
  if (!can_offer_error.IsOk()) {
    // If the profile creation is required by policy, we should sign the user
    // out since they cannot sign in to Chrome.
    if (profile_creation_required_by_policy_) {
      Signout();
    }
    OnProfileCreationDone(base::ok(nullptr));
    return false;
  }
  ShowManagementDisclaimer();
  return true;
}

void ManagedProfileCreationController::ShowManagementDisclaimer() {
  CHECK(source_profile_);
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(source_profile_)
          ->GetLastActiveBrowser();
  bool has_browser_with_tab =
      browser && WindowFeatureController::From(browser)->SupportsWindowFeature(
                     WindowFeatureController::WindowFeature::kFeatureTabStrip);

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

  if (browser && !browser->GetActiveTabInterface()) {
    // The tabs have not been initialized yet.
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
          signin::ConsentLevel::kSignin) == account_info_.GetAccountId();

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
          base::BindOnce(&SigninViewController::CloseModalSignin,
                         SigninViewController::From(browser)->AsWeakPtr()));
  SigninViewController::From(browser)->ShowModalManagedUserNoticeDialog(
      std::move(dialog_params));
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
        existing_primary_account_id == account_info_.GetAccountId());

  if (existing_primary_account_id.empty()) {
    auto* primary_account_mutator =
        GetIdentityManager()->GetPrimaryAccountMutator();
    auto set_primary_account_result =
        primary_account_mutator->SetPrimaryAccount(
            account_info_.GetAccountId(), signin::ConsentLevel::kSignin,
            access_point_);
    if (set_primary_account_result !=
        signin::PrimaryAccountMutator::PrimaryAccountError::kNoError) {
      OnProfileCreationDone(base::unexpected(
          ManagedProfileCreationFailureReason::kPrimaryAccountNotSet));
      return;
    }
  }
  OnProfileCreationDone(base::ok(source_profile_));
}

void ManagedProfileCreationController::Signout() {
  CHECK(source_profile_);
  auto* primary_account_mutator =
      GetIdentityManager()->GetPrimaryAccountMutator();
  if (account_info_.GetAccountId() == GetIdentityManager()->GetPrimaryAccountId(
                                          signin::ConsentLevel::kSignin)) {
    primary_account_mutator->RemovePrimaryAccountButKeepTokens(
        signin_metrics::ProfileSignout::
            kUserDeclinedEnterpriseManagementDisclaimer);
  }
  if (profile_creation_required_by_policy_) {
    auto* accounts_mutator = GetIdentityManager()->GetAccountsMutator();
    accounts_mutator->RemoveAccount(
        account_info_.GetAccountId(),
        signin_metrics::SourceForRefreshTokenOperation::
            kEnterpriseForcedProfileCreation_UserDecline);
  }
  OnProfileCreationDone(base::ok(nullptr));
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
        source_profile_, account_info_.GetAccountId(),
        std::vector<CoreAccountId>{}, switch_to_entry->GetPath(),
        base::BindOnce(
            &ManagedProfileCreationController::OnNewSignedInProfileCreated,
            weak_ptr_factory_.GetWeakPtr(),
            /*is_new_profile=*/false));

    return;
  }
  profile_creator_ = std::make_unique<DiceSignedInProfileCreator>(
      source_profile_, account_info_.GetAccountId(),
      std::vector<CoreAccountId>{}, profile_name,
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
    OnProfileCreationDone(base::unexpected(
        ManagedProfileCreationFailureReason::kProfileCreationFailed));
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
      OnProfileCreationDone(base::unexpected(
          ManagedProfileCreationFailureReason::kPrimaryAccountNotSet));
    }
    return;
  }

  CHECK_EQ(new_profile_primary_account_id, account_info_.GetAccountId());

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
      new_profile, is_new_profile, account_info_.GetAccountId(), nullptr);
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
  OnProfileCreationDone(base::ok(final_profile_));
}

void ManagedProfileCreationController::OnProfileCreationDone(
    base::expected<Profile*, ManagedProfileCreationFailureReason> result) {
  if (!callback_.is_null()) {
    std::move(callback_).Run(result, profile_creation_required_by_policy_);
  }
}

signin::IdentityManager*
ManagedProfileCreationController::GetIdentityManager() {
  CHECK(source_profile_);
  return IdentityManagerFactory::GetForProfile(source_profile_);
}
