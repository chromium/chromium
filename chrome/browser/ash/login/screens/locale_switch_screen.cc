// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_screen.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/locale_switch_notification.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {
namespace {

constexpr base::TimeDelta kWaitTimeout = base::Seconds(5);

// Returns whether all information needed (locale and account capabilities)
// has been fetched.
bool IsAllInfoFetched(const AccountInfo& info) {
  return !info.locale.empty() && info.capabilities.AreAllCapabilitiesKnown();
}

}  // namespace

// static
std::string LocaleSwitchScreen::GetResultString(Result result) {
  switch (result) {
    case Result::LOCALE_FETCH_FAILED:
      return "LocaleFetchFailed";
    case Result::LOCALE_FETCH_TIMEOUT:
      return "LocaleFetchTimeout";
    case Result::NO_SWITCH_NEEDED:
      return "NoSwitchNeeded";
    case Result::SWITCH_FAILED:
      return "SwitchFailed";
    case Result::SWITCH_SUCCEDED:
      return "SwitchSucceded";
    case Result::SWITCH_DELEGATED:
      return "SwitchDelegated";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

LocaleSwitchScreen::LocaleSwitchScreen(base::WeakPtr<LocaleSwitchView> view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(LocaleSwitchView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

LocaleSwitchScreen::~LocaleSwitchScreen() = default;

bool LocaleSwitchScreen::MaybeSkip(WizardContext& wizard_context) {
  if (wizard_context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Skip GAIA language sync if user specifically set language through the UI
  // on the welcome screen.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen)) {
    VLOG(1) << "Skipping GAIA language sync because user chose specific"
            << " locale on the Welcome Screen.";
    local_state->ClearPref(prefs::kOobeLocaleChangedOnWelcomeScreen);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user->HasGaiaAccount()) {
    return false;
  }

  // Switch language if logging into a managed guest session.
  if (user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    return false;
  }

  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void LocaleSwitchScreen::ShowImpl() {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user->is_profile_created());
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    std::string locale =
        profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
    DCHECK(!locale.empty());
    SwitchLocale(std::move(locale));
    return;
  }

  DCHECK(user->HasGaiaAccount());

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager_) {
    NOTREACHED();
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }

  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  refresh_token_loaded_ =
      identity_manager_->HasAccountWithRefreshToken(primary_account_id);

  if (identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          primary_account_id) != GoogleServiceAuthError::AuthErrorNone()) {
    exit_callback_.Run(Result::LOCALE_FETCH_FAILED);
    return;
  }

  identity_manager_observer_.Observe(identity_manager_.get());

  gaia_id_ = user->GetAccountId().GetGaiaId();
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_);
  if (!refresh_token_loaded_ || !IsAllInfoFetched(account_info)) {
    // Will continue from observer.
    timeout_waiter_.Start(FROM_HERE, kWaitTimeout,
                          base::BindOnce(&LocaleSwitchScreen::OnTimeout,
                                         weak_factory_.GetWeakPtr()));
    return;
  }

  std::string locale = account_info.locale;
  SwitchLocale(std::move(locale));
}

void LocaleSwitchScreen::HideImpl() {
  ResetState();
}

void LocaleSwitchScreen::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  if (account_info.gaia != gaia_id_)
    return;
  if (error == GoogleServiceAuthError::AuthErrorNone())
    return;
  ResetState();
  exit_callback_.Run(Result::LOCALE_FETCH_FAILED);
}

void LocaleSwitchScreen::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (account_info.gaia != gaia_id_ || !refresh_token_loaded_ ||
      !IsAllInfoFetched(account_info)) {
    return;
  }
  SwitchLocale(account_info.locale);
}

void LocaleSwitchScreen::OnRefreshTokensLoaded() {
  // Account information can only be guaranteed correct after refresh tokens
  // are loaded.
  refresh_token_loaded_ = true;
  OnExtendedAccountInfoUpdated(
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_));
}

void LocaleSwitchScreen::SwitchLocale(std::string locale) {
  ResetState();

  language::ConvertToActualUILocale(&locale);

  if (locale.empty() || locale == g_browser_process->GetApplicationLocale()) {
    exit_callback_.Run(Result::NO_SWITCH_NEEDED);
    return;
  }

  // Types of users that have a GAIA account and could be used during the
  // "Add Person" flow.
  static constexpr user_manager::UserType kAddPersonUserTypes[] = {
      user_manager::USER_TYPE_REGULAR, user_manager::USER_TYPE_CHILD};
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  // Don't show notification for the ephemeral logins, proceed with the default
  // flow.
  if (!chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin() &&
      context()->is_add_person_flow &&
      base::Contains(kAddPersonUserTypes, user->GetType())) {
    VLOG(1) << "Add Person flow detected, delegating locale switch decision"
            << " to the user.";
    // Delegate language switch to the notification. User will be able to
    // decide whether switch/not switch on their own.
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    locale_util::SwitchLanguageCallback callback(base::BindOnce(
        &LocaleSwitchScreen::OnLanguageChangedNotificationCallback,
        weak_factory_.GetWeakPtr()));
    LocaleSwitchNotification::Show(profile, std::move(locale),
                                   std::move(callback));
    exit_callback_.Run(Result::SWITCH_DELEGATED);
    return;
  }

  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&LocaleSwitchScreen::OnLanguageChangedCallback,
                     weak_factory_.GetWeakPtr()));
  locale_util::SwitchLanguage(
      locale,
      /*enable_locale_keyboard_layouts=*/false,  // The layouts will be synced
                                                 // instead. Also new user could
                                                 // enable required layouts from
                                                 // the settings.
      /*login_layouts_only=*/false, std::move(callback),
      ProfileManager::GetActiveUserProfile());
}

void LocaleSwitchScreen::OnLanguageChangedCallback(
    const locale_util::LanguageSwitchResult& result) {
  if (!result.success) {
    exit_callback_.Run(Result::SWITCH_FAILED);
    return;
  }

  view_->UpdateStrings();
  exit_callback_.Run(Result::SWITCH_SUCCEDED);
}

void LocaleSwitchScreen::OnLanguageChangedNotificationCallback(
    const locale_util::LanguageSwitchResult& result) {
  if (!result.success) {
    return;
  }

  view_->UpdateStrings();
}

void LocaleSwitchScreen::ResetState() {
  identity_manager_observer_.Reset();
  timeout_waiter_.AbandonAndStop();
}

void LocaleSwitchScreen::OnTimeout() {
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_);
  if (refresh_token_loaded_ && !account_info.locale.empty()) {
    // We should switch locale if locale is fetched but it timed out while
    // waiting for other account information (e.g. capabilities).
    SwitchLocale(account_info.locale);
  } else {
    ResetState();
    // If it happens during the tests - something is wrong with the test
    // configuration. Thus making it debug log.
    DLOG(ERROR) << "Timeout of the locale fetch";
    exit_callback_.Run(Result::LOCALE_FETCH_TIMEOUT);
  }
}

}  // namespace ash
