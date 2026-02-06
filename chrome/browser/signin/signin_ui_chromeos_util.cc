// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_chromeos_util.h"

#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin_ui_util {

account_manager::AccountManagerFacade::AccountAdditionSource
GetAddAccountSourceFromAccessPoint(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSettingsTurnOnSyncButton;
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kAvatarBubbleTurnOnSyncAddAccount;
    case signin_metrics::AccessPoint::kExtensions:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeExtensionAddAccount;
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoAddAccount;
    case signin_metrics::AccessPoint::kMenu:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeMenuTurnOnSync;
    case signin_metrics::AccessPoint::kAutofillDropdown:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSigninPromoAddAccount;
    case signin_metrics::AccessPoint::kGlicLaunchButton:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kGeminiInChromeReauth;
    default:
      NOTREACHED() << "Add account is requested from an unknown access point "
                   << static_cast<int>(access_point);
  }
}

account_manager::AccountManagerFacade::AccountAdditionSource
GetAccountReauthSourceFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSettingsReauthAccountButton;
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kAvatarBubbleReauthAccountButton;
    case signin_metrics::AccessPoint::kExtensions:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeExtensionReauth;
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoReauth;
    case signin_metrics::AccessPoint::kWebSignin:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kContentAreaReauth;
    case signin_metrics::AccessPoint::kMenu:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeMenuTurnOnSync;
    case signin_metrics::AccessPoint::kGlicLaunchButton:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kGeminiInChromeReauth;
    default:
      NOTREACHED() << "Reauth is requested from an unknown access point "
                   << static_cast<int>(access_point);
  }
}

}  // namespace signin_ui_util
