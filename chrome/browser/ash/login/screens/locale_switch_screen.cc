// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_screen.h"

#include "base/time/time.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kLocaleWaitTimeout = base::TimeDelta::FromSeconds(5);

}

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
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

LocaleSwitchScreen::LocaleSwitchScreen(LocaleSwitchView* view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(LocaleSwitchView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

LocaleSwitchScreen::~LocaleSwitchScreen() {
  if (view_)
    view_->Unbind();
}

bool LocaleSwitchScreen::MaybeSkip(WizardContext* wizard_context) {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user->HasGaiaAccount()) {
    return false;
  }

  // Switch language if logging into a public account.
  if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
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
    SwitchLocale(locale);
    return;
  }

  DCHECK(user->HasGaiaAccount());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    NOTREACHED();
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }

  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (!identity_manager->HasAccountWithRefreshToken(primary_account_id)) {
    exit_callback_.Run(Result::LOCALE_FETCH_FAILED);
    return;
  }

  if (identity_manager->GetErrorStateOfRefreshTokenForAccount(
          primary_account_id) != GoogleServiceAuthError::AuthErrorNone()) {
    exit_callback_.Run(Result::LOCALE_FETCH_FAILED);
    return;
  }

  identity_manager_observer_.Observe(identity_manager);

  gaia_id_ = user->GetAccountId().GetGaiaId();
  base::Optional<AccountInfo> maybe_account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(gaia_id_);
  if (!maybe_account_info.has_value() || maybe_account_info->locale.empty()) {
    // Will continue from observer.
    timeout_waiter_.Start(FROM_HERE, kLocaleWaitTimeout,
                          base::BindOnce(&LocaleSwitchScreen::OnTimeout,
                                         weak_factory_.GetWeakPtr()));
    return;
  }

  std::string locale = maybe_account_info->locale;
  SwitchLocale(std::move(locale));
}

void LocaleSwitchScreen::HideImpl() {
  ResetState();
}

void LocaleSwitchScreen::OnViewDestroyed(LocaleSwitchView* view) {
  if (view == view_)
    view_ = nullptr;
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
  if (account_info.gaia != gaia_id_ || account_info.locale.empty())
    return;
  SwitchLocale(account_info.locale);
}

void LocaleSwitchScreen::SwitchLocale(std::string locale) {
  ResetState();

  language::ConvertToActualUILocale(&locale);

  if (locale.empty() || locale == g_browser_process->GetApplicationLocale()) {
    exit_callback_.Run(Result::NO_SWITCH_NEEDED);
    return;
  }
  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&LocaleSwitchScreen::OnLanguageChangedCallback,
                     weak_factory_.GetWeakPtr()));
  locale_util::SwitchLanguage(locale,
                              true,   // enable_locale_keyboard_layouts
                              false,  // login_layouts_only
                              std::move(callback),
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

void LocaleSwitchScreen::ResetState() {
  identity_manager_observer_.Reset();
  timeout_waiter_.AbandonAndStop();
}

void LocaleSwitchScreen::OnTimeout() {
  ResetState();
  // If it happens during the tests - something is wrong with the test
  // configuration. Thus making it debug log.
  DLOG(ERROR) << "Timeout of the locale fetch";
  exit_callback_.Run(Result::LOCALE_FETCH_TIMEOUT);
}

}  // namespace chromeos
