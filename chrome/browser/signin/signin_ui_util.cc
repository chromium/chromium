// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#endif

namespace {

// Key for storing animated identity per-profile data.
const char kAnimatedIdentityKeyName[] = "animated_identity_user_data";

constexpr base::TimeDelta kDelayForCrossWindowAnimationReplay =
    base::TimeDelta::FromSeconds(5);

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

  // Returns the last time the avatar was highlighted. Returns the null time if
  // it was never shown.
  static base::TimeTicks GetAvatarLastHighlighted(Profile* profile) {
    DCHECK(profile);
    AvatarButtonUserData* data = GetForProfile(profile);
    if (!data)
      return base::TimeTicks();
    return data->avatar_last_highlighted_;
  }

  // Sets the time when the avatar was highlighted.
  static void SetAvatarLastHighlighted(Profile* profile, base::TimeTicks time) {
    DCHECK(!time.is_null());
    GetOrCreateForProfile(profile)->avatar_last_highlighted_ = time;
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
  base::TimeTicks avatar_last_highlighted_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void CreateDiceTurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
  // DiceTurnSyncOnHelper is suicidal (it will delete itself once it finishes
  // enabling sync).
  new DiceTurnSyncOnHelper(profile, browser, signin_access_point,
                           signin_promo_action, signin_reason, account_id,
                           signin_aborted_mode);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

namespace signin_ui_util {

base::string16 GetAuthenticatedUsername(Profile* profile) {
  DCHECK(profile);
  std::string user_display_name;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager->HasPrimaryAccount()) {
    user_display_name = identity_manager->GetPrimaryAccountInfo().email;
#if defined(OS_CHROMEOS)
    // See https://crbug.com/994798 for details.
    user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    // |user| may be null in tests.
    if (user)
      user_display_name = user->GetDisplayEmail();
#endif  // defined(OS_CHROMEOS)
  }

  return base::UTF8ToUTF16(user_display_name);
}

void InitializePrefsForProfile(Profile* profile) {
  if (profile->IsNewProfile()) {
    // Suppresses the upgrade tutorial for a new profile.
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown,
                                    kUpgradeWelcomeTutorialShowMax + 1);
  }
}

void ShowSigninErrorLearnMorePage(Profile* profile) {
  static const char kSigninErrorLearnMoreUrl[] =
      "https://support.google.com/chrome/answer/1181420?";
  NavigateParams params(profile, GURL(kSigninErrorLearnMoreUrl),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void EnableSyncFromPromo(Browser* browser,
                         const AccountInfo& account,
                         signin_metrics::AccessPoint access_point,
                         bool is_default_promo_account) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  internal::EnableSyncFromPromo(browser, account, access_point,
                                is_default_promo_account,
                                base::BindOnce(&CreateDiceTurnSyncOnHelper));
#else
  NOTREACHED();
#endif
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace internal {
void EnableSyncFromPromo(
    Browser* browser,
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account,
    base::OnceCallback<
        void(Profile* profile,
             Browser* browser,
             signin_metrics::AccessPoint signin_access_point,
             signin_metrics::PromoAction signin_promo_action,
             signin_metrics::Reason signin_reason,
             const CoreAccountId& account_id,
             DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode)>
        create_dice_turn_sync_on_helper_callback) {
  DCHECK(browser);
  DCHECK_NE(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN, access_point);
  Profile* profile = browser->profile();
  DCHECK(!profile->IsOffTheRecord());

  if (IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount()) {
    DVLOG(1) << "There is already a primary account.";
    return;
  }

  if (account.IsEmpty()) {
    chrome::ShowBrowserSignin(browser, access_point);
    return;
  }

  DCHECK(!account.account_id.empty());
  DCHECK(!account.email.empty());
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));

  signin_metrics::PromoAction promo_action =
      is_default_promo_account
          ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
          : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool needs_reauth_before_enable_sync =
      !identity_manager->HasAccountWithRefreshToken(account.account_id) ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account.account_id);
  if (needs_reauth_before_enable_sync) {
    browser->signin_view_controller()->ShowDiceEnableSyncTab(
        browser, access_point, promo_action, account.email);
    return;
  }

  signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
  signin_metrics::RecordSigninUserActionForAccessPoint(access_point,
                                                       promo_action);
  std::move(create_dice_turn_sync_on_helper_callback)
      .Run(profile, browser, access_point, promo_action,
           signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
           account.account_id,
           DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
}
}  // namespace internal

std::vector<AccountInfo> GetAccountsForDicePromos(Profile* profile) {
  // Fetch account ids for accounts that have a token.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::vector<AccountInfo> accounts_with_tokens =
      identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken();

  // Compute the default account.
  CoreAccountId default_account_id;
  if (identity_manager->HasPrimaryAccount()) {
    default_account_id = identity_manager->GetPrimaryAccountId();
  } else {
    // Fetch accounts in the Gaia cookies.
    auto accounts_in_cookie_jar_info =
        identity_manager->GetAccountsInCookieJar();
    std::vector<gaia::ListedAccount> signed_in_accounts =
        accounts_in_cookie_jar_info.signed_in_accounts;
    UMA_HISTOGRAM_BOOLEAN("Profile.DiceUI.GaiaAccountsStale",
                          !accounts_in_cookie_jar_info.accounts_are_fresh);

    if (accounts_in_cookie_jar_info.accounts_are_fresh &&
        !signed_in_accounts.empty())
      default_account_id = signed_in_accounts[0].id;
  }

  // Fetch account information for each id and make sure that the first account
  // in the list matches the first account in the Gaia cookies (if available).
  std::vector<AccountInfo> accounts;
  for (auto& account_info : accounts_with_tokens) {
    DCHECK(!account_info.IsEmpty());
    if (!signin::IsUsernameAllowedByPatternFromPrefs(
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

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

base::string16 GetShortProfileIdentityToDisplay(
    const ProfileAttributesEntry& profile_attributes_entry,
    Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_info =
      identity_manager->GetUnconsentedPrimaryAccountInfo();
  // If there's no unconsented primary account, simply return the name of the
  // profile according to profile attributes.
  if (core_info.IsEmpty())
    return profile_attributes_entry.GetName();

  base::Optional<AccountInfo> extended_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              core_info.account_id);
  // If there's no given name available, return the user email.
  if (!extended_info.has_value() || extended_info->given_name.empty())
    return base::UTF8ToUTF16(core_info.email);

  return base::UTF8ToUTF16(extended_info->given_name);
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

void RecordAnimatedIdentityTriggered(Profile* profile) {
  AvatarButtonUserData::SetAnimatedIdentityLastShown(profile,
                                                     base::TimeTicks::Now());
}

void RecordAvatarIconHighlighted(Profile* profile) {
  base::RecordAction(base::UserMetricsAction("AvatarToolbarButtonHighlighted"));
  AvatarButtonUserData::SetAvatarLastHighlighted(profile,
                                                 base::TimeTicks::Now());
}

void RecordProfileMenuViewShown(Profile* profile) {
  base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened"));
  if (profile->IsRegularProfile()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Regular"));
  } else if (profile->IsGuestSession()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Guest"));
  } else if (profile->IsIncognitoProfile()) {
    base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened_Incognito"));
  }

  base::TimeTicks last_shown =
      AvatarButtonUserData::GetAnimatedIdentityLastShown(profile);
  if (!last_shown.is_null()) {
    base::UmaHistogramLongTimes("Profile.Menu.OpenedAfterAvatarAnimation",
                                base::TimeTicks::Now() - last_shown);
  }

  last_shown = AvatarButtonUserData::GetAvatarLastHighlighted(profile);
  if (!last_shown.is_null()) {
    base::UmaHistogramLongTimes("Profile.Menu.OpenedAfterAvatarHighlight",
                                base::TimeTicks::Now() - last_shown);
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

}  // namespace signin_ui_util
