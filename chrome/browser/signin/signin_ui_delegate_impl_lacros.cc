// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_lacros.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin_ui_util {
namespace {

account_manager::AccountManagerFacade::AccountAdditionSource
GetAddAccountSourceFromAccessPoint(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSettingsTurnOnSyncButton;
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kAvatarBubbleTurnOnSyncAddAccount;
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeExtensionAddAccount;
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoAddAccount;
    default:
      NOTREACHED() << "Add account is requested from an unknown access point "
                   << static_cast<int>(access_point);
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kMaxValue;
  }
}

account_manager::AccountManagerFacade::AccountAdditionSource
GetAccountReauthSourceFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSettingsReauthAccountButton;
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kAvatarBubbleReauthAccountButton;
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeExtensionReauth;
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoReauth;
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kContentAreaReauth;
    default:
      NOTREACHED() << "Reauth is requested from an unknown access point "
                   << static_cast<int>(access_point);
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kMaxValue;
  }
}
}  // namespace

void SigninUiDelegateImplLacros::ShowSigninUI(
    Profile* profile,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  if (enable_sync) {
    signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
  }

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  auto on_completion_callback =
      base::BindOnce(&SigninUiDelegateImplLacros::OnAccountAdded,
                     // base::Unretained() is fine because
                     // SigninUiDelegateImplLacros is a singleton.
                     base::Unretained(this), enable_sync, /*is_reauth=*/false,
                     profile->GetPath(), access_point, promo_action);
  signin_manager->StartLacrosSigninFlow(
      profile->GetPath(),
      g_browser_process->profile_manager()->GetAccountProfileMapper(),
      AccountReconcilorFactory::GetForProfile(profile)
          ->GetConsistencyCookieManager(),
      GetAddAccountSourceFromAccessPoint(access_point),
      std::move(on_completion_callback));
}

void SigninUiDelegateImplLacros::ShowReauthUI(
    Profile* profile,
    const std::string& email,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  if (enable_sync) {
    signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
  }

  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  base::OnceClosure reauth_completed_closure =
      base::BindOnce(&SigninUiDelegateImplLacros::OnReauthComplete,
                     // base::Unretained() is fine because
                     // SigninUiDelegateImplLacros is a singleton.
                     base::Unretained(this), enable_sync,
                     account_reconcilor->GetConsistencyCookieManager()
                         ->CreateScopedAccountUpdate(),
                     profile->GetPath(), access_point, promo_action, email);
  account_manager::AccountManagerFacade* account_manager_facade =
      ::GetAccountManagerFacade(profile->GetPath().value());
  account_manager_facade->ShowReauthAccountDialog(
      GetAccountReauthSourceFromAccessPoint(access_point), email,
      std::move(reauth_completed_closure));
}

void SigninUiDelegateImplLacros::OnAccountAdded(
    bool enable_sync,
    bool is_reauth,
    const base::FilePath& profile_path,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const CoreAccountId& account_id) {
  if (!enable_sync || account_id.empty())
    return;

  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (!profile)
    return;

  Browser* browser = EnsureBrowser(profile);
  if (!browser)
    return;

  ShowTurnSyncOnUI(profile, access_point, promo_action,
                   is_reauth ? signin_metrics::Reason::kReauthentication
                             : signin_metrics::Reason::kSigninPrimaryAccount,
                   account_id,
                   is_reauth
                       ? TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT
                       : TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
}

void SigninUiDelegateImplLacros::OnReauthComplete(
    bool enable_sync,
    signin::ConsistencyCookieManager::ScopedAccountUpdate&& update,
    const base::FilePath& profile_path,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email) {
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (!profile)
    return;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  OnAccountAdded(enable_sync, /*is_reauth=*/true, profile_path, access_point,
                 promo_action,
                 identity_manager->FindExtendedAccountInfoByEmailAddress(email)
                     .account_id);
}

}  // namespace signin_ui_util
