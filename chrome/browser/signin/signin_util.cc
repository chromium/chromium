// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#endif  // BUILDFLAG(IS_LINUX) ||  BUILDFLAG(IS_MAC) ||  BUILDFLAG(IS_WIN)

namespace signin_util {

namespace {

enum ForceSigninPolicyCache {
  NOT_CACHED = 0,
  ENABLE,
  DISABLE
} g_is_force_signin_enabled_cache = NOT_CACHED;

void SetForceSigninPolicy(bool enable) {
  g_is_force_signin_enabled_cache = enable ? ENABLE : DISABLE;
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSigninErrorDialogOkButtonId);

ScopedForceSigninSetterForTesting::ScopedForceSigninSetterForTesting(
    bool enable) {
  SetForceSigninForTesting(enable);  // IN-TEST
}

ScopedForceSigninSetterForTesting::~ScopedForceSigninSetterForTesting() {
  ResetForceSigninForTesting();  // IN-TEST
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
CookiesMover::CookiesMover(base::WeakPtr<Profile> source_profile,
                           base::WeakPtr<Profile> destination_profile,
                           base::OnceCallback<void()> callback)
    : url_(source_profile->GetPrefs()->GetString(
          prefs::kSigninInterceptionIDPCookiesUrl)),
      source_profile_(std::move(source_profile)),
      destination_profile_(std::move(destination_profile)),
      callback_(std::move(callback)) {
  CHECK(callback_);
}

CookiesMover::~CookiesMover() = default;

void CookiesMover::StartMovingCookies() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  bool allow_cookies_to_be_moved = base::FeatureList::IsEnabled(
      profile_management::features::kThirdPartyProfileManagement);
#else
  bool allow_cookies_to_be_moved = false;
#endif
  if (!allow_cookies_to_be_moved || url_.is_empty() || !url_.is_valid()) {
    std::move(callback_).Run();
    return;
  }

  source_profile_->GetPrefs()->ClearPref(
      prefs::kSigninInterceptionIDPCookiesUrl);
  source_profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url_, net::CookieOptions::MakeAllInclusive(),
                      net::CookiePartitionKeyCollection::Todo(),
                      base::BindOnce(&CookiesMover::OnCookiesReceived,
                                     weak_pointer_factory_.GetWeakPtr()));
}

void CookiesMover::OnCookiesReceived(
    const std::vector<net::CookieWithAccessResult>& included,
    const std::vector<net::CookieWithAccessResult>& excluded) {
  // If either profile was destroyed, stop the operation.
  if (source_profile_.WasInvalidated() ||
      destination_profile_.WasInvalidated()) {
    std::move(callback_).Run();
    return;
  }
  // We expected 2 * `cookies.size()` actions since we have to set the cookie at
  // destination and delete it from the source.
  base::RepeatingClosure barrier = base::BarrierClosure(
      included.size() * 2, base::BindOnce(&CookiesMover::OnCookiesMoved,
                                          weak_pointer_factory_.GetWeakPtr()));
  auto* source_cookie_manager = source_profile_->GetDefaultStoragePartition()
                                    ->GetCookieManagerForBrowserProcess();
  auto* destination_cookie_manager =
      destination_profile_->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  for (const auto& [cookie, _] : included) {
    destination_cookie_manager->SetCanonicalCookie(
        cookie, url_, net::CookieOptions::MakeAllInclusive(),
        base::IgnoreArgs<net::CookieAccessResult>(barrier));
    source_cookie_manager->DeleteCanonicalCookie(
        cookie, base::IgnoreArgs<bool>(barrier));
  }
}

void CookiesMover::OnCookiesMoved() {
  std::move(callback_).Run();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

bool IsForceSigninEnabled() {
  if (g_is_force_signin_enabled_cache == NOT_CACHED) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs) {
      SetForceSigninPolicy(prefs->GetBoolean(prefs::kForceBrowserSignin));
    } else {
      return false;
    }
  }
  return (g_is_force_signin_enabled_cache == ENABLE);
}

void SetForceSigninForTesting(bool enable) {
  SetForceSigninPolicy(enable);
}

void ResetForceSigninForTesting() {
  g_is_force_signin_enabled_cache = NOT_CACHED;
}

bool IsProfileDeletionAllowed(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
// Returns true if managed accounts signin are required to create a new profile
// by policies set in `profile`.
bool IsProfileSeparationEnforcedByProfile(
    Profile* profile,
    const std::string& intercepted_account_email) {
  if (!intercepted_account_email.empty() &&
      !IsAccountExemptedFromEnterpriseProfileSeparation(
          profile, intercepted_account_email)) {
    return true;
  }
  std::string legacy_policy_for_current_profile =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);
  bool enforced_by_existing_profile = base::StartsWith(
      legacy_policy_for_current_profile, "primary_account_strict");
  bool enforced_at_machine_level =
      base::StartsWith(legacy_policy_for_current_profile, "primary_account") &&
      profile->GetPrefs()->GetBoolean(
          prefs::kManagedAccountsSigninRestrictionScopeMachine);
  return enforced_by_existing_profile || enforced_at_machine_level;
}

// Returns true if profile separation is enforced by
// `intercepted_account_separation_policies`.
bool IsProfileSeparationEnforcedByPolicies(
    const policy::ProfileSeparationPolicies&
        intercepted_account_separation_policies) {
  if (intercepted_account_separation_policies.profile_separation_settings()
          .value_or(policy::ProfileSeparationSettings::SUGGESTED) ==
      policy::ProfileSeparationSettings::ENFORCED) {
    return true;
  }

  std::string legacy_policy_for_intercepted_profile =
      intercepted_account_separation_policies
          .managed_accounts_signin_restrictions()
          .value_or(std::string());
  return base::StartsWith(legacy_policy_for_intercepted_profile,
                          "primary_account");
}

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const policy::ProfileSeparationPolicies&
        intercepted_account_separation_policies) {
  // We should not move managed data.
  if (enterprise_util::UserAcceptedAccountManagement(profile)) {
    return false;
  }

  std::string legacy_policy_for_intercepted_profile =
      intercepted_account_separation_policies
          .managed_accounts_signin_restrictions()
          .value_or(std::string());
  std::string legacy_policy_for_current_profile =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);
  bool allowed_by_existing_profile =
      legacy_policy_for_current_profile.empty() ||
      legacy_policy_for_current_profile == "none" ||
      base::EndsWith(legacy_policy_for_current_profile, "keep_existing_data");
  bool allowed_by_intercepted_account =
      intercepted_account_separation_policies
              .profile_separation_data_migration_settings()
              .value_or(policy::ProfileSeparationDataMigrationSettings::
                            USER_OPT_IN) !=
          policy::ProfileSeparationDataMigrationSettings::ALWAYS_SEPARATE &&
      (legacy_policy_for_intercepted_profile.empty() ||
       legacy_policy_for_intercepted_profile == "none" ||
       base::EndsWith(legacy_policy_for_intercepted_profile,
                      "keep_existing_data"));
  return allowed_by_existing_profile && allowed_by_intercepted_account;
}

bool IsAccountExemptedFromEnterpriseProfileSeparation(
    Profile* profile,
    const std::string& email) {
  if (profile->GetPrefs()
          ->FindPreference(prefs::kProfileSeparationDomainExceptionList)
          ->IsDefaultValue()) {
    return true;
  }

  const std::string domain = gaia::ExtractDomainName(email);
  const auto& allowed_domains = profile->GetPrefs()->GetList(
      prefs::kProfileSeparationDomainExceptionList);
  return base::Contains(allowed_domains, base::Value(domain));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created) {
  base::UmaHistogramBoolean(
      enforced_by_policy
          ? "Signin.Enterprise.WorkProfile.ProfileCreatedWithPolicySet"
          : "Signin.Enterprise.WorkProfile.ProfileCreatedwithPolicyUnset",
      created);
}

#endif  // !BUILDFLAG(IS_ANDROID)

PrimaryAccountError SetPrimaryAccountWithInvalidToken(
    Profile* profile,
    const std::string& user_email,
    const GaiaId& gaia_id,
    bool is_under_advanced_protection,
    signin_metrics::AccessPoint access_point,
    signin_metrics::SourceForRefreshTokenOperation source) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  CHECK(identity_manager->FindExtendedAccountInfoByEmailAddress(user_email)
            .IsEmpty());
  CHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  DVLOG(1) << "Adding user with gaia id <" << gaia_id << "> and email <"
           << user_email << "> with invalid refresh token.";

  // Lock AccountReconcilor temporarily to prevent AddOrUpdateAccount failure
  // since we have an invalid refresh token.
  AccountReconcilor::Lock account_reconcilor_lock(
      AccountReconcilorFactory::GetForProfile(profile));

  CoreAccountId account_id =
      identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
          gaia_id, user_email, GaiaConstants::kInvalidRefreshToken,
          is_under_advanced_protection, access_point, source);

  DVLOG(1) << "Account id <" << account_id.ToString()
           << "> has been added to the profile with invalid token.";

  auto set_primary_account_result =
      identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account_id, signin::ConsentLevel::kSignin);
  DVLOG(1) << "Operation of setting account id <" << account_id.ToString()
           << "> received the following result: "
           << static_cast<int>(set_primary_account_result);

  return set_primary_account_result;
}

bool IsSigninPending(signin::IdentityManager* identity_manager) {
  return !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
             identity_manager->GetPrimaryAccountId(
                 signin::ConsentLevel::kSignin));
}

SignedInState GetSignedInState(
    const signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return SignedInState::kSignedOut;
  }

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSync))) {
      return SignedInState::kSyncPaused;
    }
    return SignedInState::kSyncing;
  }

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
               identity_manager->GetPrimaryAccountId(
                   signin::ConsentLevel::kSignin))
               ? SignedInState::kSignInPending
               : SignedInState::kSignedIn;
  }

  // Not signed, but at least one account is signed in on the web.
  if (!identity_manager->GetAccountsWithRefreshTokens().empty()) {
    return SignedInState::kWebOnlySignedIn;
  }

  return SignedInState::kSignedOut;
}

std::string SignedInStateToString(SignedInState state) {
  switch (state) {
    case SignedInState::kSignedOut:
      return "Signed Out";
    case SignedInState::kSignedIn:
      return "Signed In";
    case SignedInState::kSyncing:
      return "Syncing";
    case SignedInState::kSignInPending:
      return "Sign-in Pending";
    case SignedInState::kWebOnlySignedIn:
      return "Web Only Signed In";
    case SignedInState::kSyncPaused:
      return "Sync Paused";
    default:
      NOTREACHED();
  }
}

bool IsSyncingUserSelectableTypesAllowedByPolicy(
    const syncer::SyncService* sync_service,
    const syncer::UserSelectableTypeSet& types) {
  if (!sync_service) {
    return false;
  }

  if (sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    return false;
  }

  for (auto type : types) {
    if (sync_service->GetUserSettings()->IsTypeManagedByPolicy(type) ||
        sync_service->GetUserSettings()->IsTypeManagedByCustodian(type)) {
      return false;
    }
  }

  return true;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool HasExplicitlyDisabledHistorySync(
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager) {
  switch (GetSignedInState(identity_manager)) {
    case SignedInState::kSignedOut:
    case SignedInState::kWebOnlySignedIn:
      // If the user is signed out, we cannot know if the toggles were
      // interacted with or not.
      NOTREACHED();
    case SignedInState::kSignedIn:
    case SignedInState::kSyncing:
    case SignedInState::kSignInPending:
    case SignedInState::kSyncPaused:
      break;
  }

  if (!sync_service) {
    return false;
  }

  for (auto type :
       {syncer::UserSelectableType::kHistory, syncer::UserSelectableType::kTabs,
        syncer::UserSelectableType::kSavedTabGroups}) {
    if (sync_service->GetUserSettings()->GetTypePrefStateForAccount(type) ==
        syncer::SyncUserSettings::UserSelectableTypePrefState::kDisabled) {
      return true;
    }
  }

  return false;
}

ShouldShowHistorySyncOptinResult ShouldShowHistorySyncOptinScreen(
    Profile& profile) {
  if (GetSignedInState(IdentityManagerFactory::GetForProfile(&profile)) !=
      signin_util::SignedInState::kSignedIn) {
    return ShouldShowHistorySyncOptinResult::kSkipUserNotSignedIn;
  }

  syncer::UserSelectableTypeSet required_types(
      {syncer::UserSelectableType::kHistory, syncer::UserSelectableType::kTabs,
       syncer::UserSelectableType::kSavedTabGroups});
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);
  if (!IsSyncingUserSelectableTypesAllowedByPolicy(sync_service,
                                                   required_types)) {
    return ShouldShowHistorySyncOptinResult::kSkipSyncForbidden;
  }

  // Sync service must exist, otherwise the method would have exited already.
  CHECK(sync_service);
  // Note: Post migration these preferences will be set by a single
  // settings toggle and are expected to have the same value.
  bool all_types_enabled =
      sync_service->GetUserSettings()->GetSelectedTypes().HasAll(
          required_types);
  if (all_types_enabled) {
    return ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn;
  }

  return ShouldShowHistorySyncOptinResult::kShow;
}

void EnableHistorySync(syncer::SyncService* sync_service) {
  CHECK(sync_service);

  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, /*is_type_on=*/true);
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, /*is_type_on=*/true);
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, /*is_type_on=*/true);
}

bool IsValidAccessPointForHistoryOptinScreen(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case (signin_metrics::AccessPoint::kExtensionInstallBubble):
    case (signin_metrics::AccessPoint::kBookmarkBubble):
    case (signin_metrics::AccessPoint::kRecentTabs):
    case (signin_metrics::AccessPoint::kCollaborationJoinTabGroup):
    case (signin_metrics::AccessPoint::kCollaborationShareTabGroup):
    case (signin_metrics::AccessPoint::kPasswordBubble):
    case (signin_metrics::AccessPoint::kAddressBubble):
      return false;
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkManager:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReadingList:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      return true;
  }
}

bool ShouldShowAvatarSyncPromo(Profile* profile) {
  CHECK(switches::IsAvatarSyncPromoFeatureEnabled());

  // Do not show the promo for users that are not signed in. (E.g. Signed out,
  // Signin Pending or already syncing).
  if (GetSignedInState(IdentityManagerFactory::GetForProfile(profile)) !=
      signin_util::SignedInState::kSignedIn) {
    return false;
  }

  // SyncService should be usable.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  if (sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager->AreRefreshTokensLoaded());
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  // Do not show the promo for non signed in accounts, or managed accounts.
  if (account_info.IsEmpty() ||
      account_info.IsManaged() != signin::Tribool::kFalse) {
    return false;
  }

  // Do not show the promo if there was a previously syncing account that does
  // not match the currently signed in one.
  PrefService* pref_service = profile->GetPrefs();
  GaiaId previously_syncing_gaia_id =
      GaiaId(pref_service->GetString(prefs::kGoogleServicesLastSyncingGaiaId));
  if (IsCrossAccountError(profile, account_info.gaia)) {
    return false;
  }

  // For non-dice users, do not show the promo for users that have been signed
  // for a short period of time.
  if (pref_service->GetBoolean(prefs::kExplicitBrowserSignin)) {
    const base::Time last_changed = base::Time::FromSecondsSinceUnixEpoch(
        pref_service->GetDouble(prefs::kGaiaCookieChangedTime));
    if (last_changed.is_null() ||
        (base::Time::Now() - last_changed <
         switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam())) {
      return false;
    }
  }

  return true;
}

void ShowErrorDialogWithMessage(Browser* browser, int error_message_id) {
  if (!browser) {
    return;
  }

  auto dialog_model =
      ui::DialogModel::Builder()
          .AddParagraph(ui::DialogModelLabel(error_message_id),
                        /*header=*/std::u16string(), kSigninErrorDialogId)
          .AddOkButton(base::DoNothing(),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_OK))
                           .SetId(kSigninErrorDialogOkButtonId))
          .Build();

  chrome::ShowBrowserModal(browser, std::move(dialog_model));
}
#endif  // BUILDFLAG(IS_LINUX) ||  BUILDFLAG(IS_MAC) ||  BUILDFLAG(IS_WIN)

}  // namespace signin_util
