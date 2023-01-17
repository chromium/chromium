// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/signin_ui_delegate_impl_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace signin_ui_util {
namespace {

// Key for storing animated identity per-profile data.
const char kAnimatedIdentityKeyName[] = "animated_identity_user_data";

constexpr base::TimeDelta kDelayForCrossWindowAnimationReplay =
    base::Seconds(5);

// UserData attached to the user profile, keeping track of the last time the
// animation was shown to the user.
class AvatarButtonUserData : public base::SupportsUserData::Data {
 public:
  ~AvatarButtonUserData() override = default;

  // Returns the  last time the animated identity was shown. Returns the null
  // time if it was never shown.
  static base::TimeTicks GetAnimatedIdentityLastShown(Profile* profile) {
    DCHECK(profile);
    AvatarButtonUserData* data = GetForProfile(profile);
    if (!data)
      return base::TimeTicks();
    return data->animated_identity_last_shown_;
  }

  // Sets the time when the animated identity was shown.
  static void SetAnimatedIdentityLastShown(Profile* profile,
                                           base::TimeTicks time) {
    DCHECK(!time.is_null());
    GetOrCreateForProfile(profile)->animated_identity_last_shown_ = time;
  }

 private:
  // Returns nullptr if there is no AvatarButtonUserData attached to the
  // profile.
  static AvatarButtonUserData* GetForProfile(Profile* profile) {
    return static_cast<AvatarButtonUserData*>(
        profile->GetUserData(kAnimatedIdentityKeyName));
  }

  // Never returns nullptr.
  static AvatarButtonUserData* GetOrCreateForProfile(Profile* profile) {
    DCHECK(profile);
    AvatarButtonUserData* existing_data = GetForProfile(profile);
    if (existing_data)
      return existing_data;

    auto new_data = std::make_unique<AvatarButtonUserData>();
    auto* new_data_ptr = new_data.get();
    profile->SetUserData(kAnimatedIdentityKeyName, std::move(new_data));
    return new_data_ptr;
  }

  base::TimeTicks animated_identity_last_shown_;
};

std::string GetReauthAccessPointHistogramSuffix(
    signin_metrics::ReauthAccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::ReauthAccessPoint::kUnknown:
      NOTREACHED();
      return std::string();
    case signin_metrics::ReauthAccessPoint::kAutofillDropdown:
      return "ToFillPassword";
    case signin_metrics::ReauthAccessPoint::kPasswordSaveBubble:
      return "ToSaveOrUpdatePassword";
    case signin_metrics::ReauthAccessPoint::kPasswordSettings:
      return "ToManageInSettings";
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown:
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu:
      return "ToGeneratePassword";
    case signin_metrics::ReauthAccessPoint::kPasswordMoveBubble:
      return "ToMovePassword";
    case signin_metrics::ReauthAccessPoint::kPasswordSaveLocallyBubble:
      return "ToSavePasswordLocallyThenMove";
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

SigninUiDelegate* g_signin_ui_delegate_for_testing = nullptr;

SigninUiDelegate* GetSigninUiDelegate() {
  if (g_signin_ui_delegate_for_testing)
    return g_signin_ui_delegate_for_testing;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  static SigninUiDelegateImplLacros delegate;
#else
  static SigninUiDelegateImplDice delegate;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return &delegate;
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

std::u16string GetAuthenticatedUsername(Profile* profile) {
  DCHECK(profile);
  std::string user_display_name;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    user_display_name =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .email;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // See https://crbug.com/994798 for details.
    user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile);
    // |user| may be null in tests.
    if (user)
      user_display_name = user->GetDisplayEmail();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  return base::UTF8ToUTF16(user_display_name);
}

void ShowSigninErrorLearnMorePage(Profile* profile) {
  static const char kSigninErrorLearnMoreUrl[] =
      "https://support.google.com/chrome/answer/1181420?";
  NavigateParams params(profile, GURL(kSigninErrorLearnMoreUrl),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ShowReauthForPrimaryAccountWithAuthError(
    Profile* profile,
    signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, sync errors are fixed by re-signing into the OS.
  NOTREACHED();
#else
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  DCHECK(!primary_account_info.IsEmpty());
  DCHECK(identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
      primary_account_info.account_id));
  ShowReauthForAccount(profile, primary_account_info.email, access_point);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowReauthForAccount(Profile* profile,
                          const std::string& email,
                          signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only `ACCESS_POINT_WEB_SIGNIN` is supported, because `kContentAreaReauth`
  // is hardcoded.
  DCHECK_EQ(access_point, signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
  ::GetAccountManagerFacade(profile->GetPath().value())
      ->ShowReauthAccountDialog(account_manager::AccountManagerFacade::
                                    AccountAdditionSource::kContentAreaReauth,
                                email, base::OnceClosure());
#else
  // Pass `false` for `enable_sync`, as this function is not expected to start a
  // sync setup flow after the reauth.
  GetSigninUiDelegate()->ShowReauthUI(
      profile, email,
      /*enable_sync=*/false, access_point,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
#endif
}

void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#else
  // There is no sign-in flow for guest or system profile.
  if (profile->IsGuestSession() || profile->IsSystemProfile())
    return;
  // Locked profile should be unlocked with UserManager only.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return;
  }

  // This may be called in incognito. Redirect to the original profile.
  profile = profile->GetOriginalProfile();

  if (email_hint.empty()) {
    // Add a new account.
    GetSigninUiDelegate()->ShowSigninUI(
        profile, enable_sync,
        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    return;
  }

  // Re-authenticate an existing account.
  GetSigninUiDelegate()->ShowReauthUI(
      profile, email_hint, enable_sync,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void EnableSyncFromSingleAccountPromo(
    Profile* profile,
    const CoreAccountInfo& account,
    signin_metrics::AccessPoint access_point) {
  EnableSyncFromMultiAccountPromo(profile, account, access_point,
                                  /*is_default_promo_account=*/true);
}

void EnableSyncFromMultiAccountPromo(Profile* profile,
                                     const CoreAccountInfo& account,
                                     signin_metrics::AccessPoint access_point,
                                     bool is_default_promo_account) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK_NE(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN, access_point);
  DCHECK(!profile->IsOffTheRecord());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    DVLOG(1) << "There is already a primary account.";
    return;
  }

  if (account.IsEmpty()) {
    signin_metrics::PromoAction new_account_promo_action =
        identity_manager->GetAccountsWithRefreshTokens().empty()
            ? signin_metrics::PromoAction::
                  PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
            : signin_metrics::PromoAction::
                  PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT;
    GetSigninUiDelegate()->ShowSigninUI(profile, /*enable_sync=*/true,
                                        access_point, new_account_promo_action);
    return;
  }

  DCHECK(!account.account_id.empty());
  DCHECK(!account.email.empty());
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) ||
         AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));

  signin_metrics::PromoAction existing_account_promo_action =
      is_default_promo_account
          ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
          : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;

  bool needs_reauth_before_enable_sync =
      !identity_manager->HasAccountWithRefreshToken(account.account_id) ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account.account_id);
  if (needs_reauth_before_enable_sync) {
    GetSigninUiDelegate()->ShowReauthUI(profile, account.email,
                                        /*enable_sync=*/true, access_point,
                                        existing_account_promo_action);
    return;
  }

  signin_metrics::LogSigninAccessPointStarted(access_point,
                                              existing_account_promo_action);
  signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
  GetSigninUiDelegate()->ShowTurnSyncOnUI(
      profile, access_point, existing_account_promo_action,
      signin_metrics::Reason::kSigninPrimaryAccount, account.account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

std::vector<AccountInfo> GetOrderedAccountsForDisplay(
    Profile* profile,
    bool restrict_to_accounts_eligible_for_sync) {
  // Fetch account ids for accounts that have a token.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::vector<AccountInfo> accounts_with_tokens =
      identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken();

  // Compute the default account.
  CoreAccountId default_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  // Fetch account information for each id and make sure that the first account
  // in the list matches the unconsented primary account (if available).
  std::vector<AccountInfo> accounts;
  for (auto& account_info : accounts_with_tokens) {
    DCHECK(!account_info.IsEmpty());
    if (restrict_to_accounts_eligible_for_sync &&
        !signin::IsUsernameAllowedByPatternFromPrefs(
            g_browser_process->local_state(), account_info.email)) {
      continue;
    }
    if (account_info.account_id == default_account_id)
      accounts.insert(accounts.begin(), std::move(account_info));
    else
      accounts.push_back(std::move(account_info));
  }
  return accounts;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)

AccountInfo GetSingleAccountForPromos(Profile* profile) {
  std::vector<AccountInfo> accounts = GetOrderedAccountsForDisplay(
      profile, /*restrict_to_accounts_eligible_for_sync=*/true);
  if (!accounts.empty())
    return accounts[0];
  return AccountInfo();
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

std::u16string GetShortProfileIdentityToDisplay(
    const ProfileAttributesEntry& profile_attributes_entry,
    Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  // If there's no unconsented primary account, simply return the name of the
  // profile according to profile attributes.
  if (core_info.IsEmpty())
    return profile_attributes_entry.GetName();

  AccountInfo extended_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          core_info.account_id);
  // If there's no given name available, return the user email.
  if (extended_info.given_name.empty())
    return base::UTF8ToUTF16(core_info.email);

  return base::UTF8ToUTF16(extended_info.given_name);
}

std::string GetAllowedDomain(std::string signin_pattern) {
  std::vector<std::string> splitted_signin_pattern = base::SplitString(
      signin_pattern, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // There are more than one '@'s in the pattern.
  if (splitted_signin_pattern.size() != 2)
    return std::string();

  std::string domain = splitted_signin_pattern[1];

  // Trims tailing '$' if existed.
  if (!domain.empty() && domain.back() == '$')
    domain.pop_back();

  // Trims tailing '\E' if existed.
  if (domain.size() > 1 &&
      base::EndsWith(domain, "\\E", base::CompareCase::SENSITIVE))
    domain.erase(domain.size() - 2);

  // Check if there is any special character in the domain. Note that
  // jsmith@[192.168.2.1] is not supported.
  if (!re2::RE2::FullMatch(domain, "[a-zA-Z0-9\\-.]+"))
    return std::string();

  return domain;
}

bool ShouldShowAnimatedIdentityOnOpeningWindow(
    const ProfileAttributesStorage& profile_attributes_storage,
    Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->AreRefreshTokensLoaded());

  base::TimeTicks animation_last_shown =
      AvatarButtonUserData::GetAnimatedIdentityLastShown(profile);
  // When a new window is created, only show the animation if it was never shown
  // for this profile, or if it was shown in another window in the last few
  // seconds (because the user may have missed it).
  if (!animation_last_shown.is_null() &&
      base::TimeTicks::Now() - animation_last_shown >
          kDelayForCrossWindowAnimationReplay) {
    return false;
  }

  // Show the user identity for users with multiple profiles.
  if (profile_attributes_storage.GetNumberOfProfiles() > 1) {
    return true;
  }

  // Show the user identity for users with multiple signed-in accounts.
  return identity_manager->GetAccountsWithRefreshTokens().size() > 1;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
base::AutoReset<SigninUiDelegate*> SetSigninUiDelegateForTesting(  // IN-TEST
    SigninUiDelegate* delegate) {
  return base::AutoReset<SigninUiDelegate*>(&g_signin_ui_delegate_for_testing,
                                            delegate);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

void RecordAnimatedIdentityTriggered(Profile* profile) {
  AvatarButtonUserData::SetAnimatedIdentityLastShown(profile,
                                                     base::TimeTicks::Now());
}

void RecordAvatarIconHighlighted(Profile* profile) {
  base::RecordAction(base::UserMetricsAction("AvatarToolbarButtonHighlighted"));
}

void RecordProfileMenuViewShown(Profile* profile) {
  base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened"));
  if (profile->IsRegularProfile()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Regular"));
    // Record usage for profile switch promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(profile)
        ->NotifyEvent("profile_menu_shown");
  } else if (profile->IsGuestSession()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Guest"));
  } else if (profile->IsIncognitoProfile()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Incognito"));
  }
}

void RecordProfileMenuClick(Profile* profile) {
  base::RecordAction(
      base::UserMetricsAction("ProfileMenu_ActionableItemClicked"));
  if (profile->IsRegularProfile()) {
    base::RecordAction(
        base::UserMetricsAction("ProfileMenu_ActionableItemClicked_Regular"));
  } else if (profile->IsGuestSession()) {
    base::RecordAction(
        base::UserMetricsAction("ProfileMenu_ActionableItemClicked_Guest"));
  } else if (profile->IsIncognitoProfile()) {
    base::RecordAction(
        base::UserMetricsAction("ProfileMenu_ActionableItemClicked_Incognito"));
  }
}

void RecordTransactionalReauthResult(
    signin_metrics::ReauthAccessPoint access_point,
    signin::ReauthResult result) {
  const char kHistogramName[] = "Signin.TransactionalReauthResult";
  base::UmaHistogramEnumeration(kHistogramName, result);

  std::string access_point_suffix =
      GetReauthAccessPointHistogramSuffix(access_point);
  if (!access_point_suffix.empty()) {
    std::string suffixed_histogram_name =
        base::StrCat({kHistogramName, ".", access_point_suffix});
    base::UmaHistogramEnumeration(suffixed_histogram_name, result);
  }
}

void RecordTransactionalReauthUserAction(
    signin_metrics::ReauthAccessPoint access_point,
    SigninReauthViewController::UserAction user_action) {
  const char kHistogramName[] = "Signin.TransactionalReauthUserAction";
  base::UmaHistogramEnumeration(kHistogramName, user_action);

  std::string access_point_suffix =
      GetReauthAccessPointHistogramSuffix(access_point);
  if (!access_point_suffix.empty()) {
    std::string suffixed_histogram_name =
        base::StrCat({kHistogramName, ".", access_point_suffix});
    base::UmaHistogramEnumeration(suffixed_histogram_name, user_action);
  }
}

}  // namespace signin_ui_util
