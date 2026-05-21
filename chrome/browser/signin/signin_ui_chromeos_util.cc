// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_chromeos_util.h"

#include "base/notreached.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin_ui_util {

account_manager::AccountAdditionSource GetAddAccountSourceFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kSettingsAutofillAndPasswords:
      return account_manager::AccountAdditionSource::
          kChromeSettingsTurnOnSyncButton;
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
      return account_manager::AccountAdditionSource::
          kAvatarBubbleTurnOnSyncAddAccount;
    case signin_metrics::AccessPoint::kExtensions:
      return account_manager::AccountAdditionSource::kChromeExtensionAddAccount;
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
      return account_manager::AccountAdditionSource::kChromeSyncPromoAddAccount;
    case signin_metrics::AccessPoint::kMenu:
      return account_manager::AccountAdditionSource::kChromeMenuTurnOnSync;
    case signin_metrics::AccessPoint::kAutofillDropdown:
      return account_manager::AccountAdditionSource::
          kChromeSigninPromoAddAccount;
    case signin_metrics::AccessPoint::kGlicLaunchButton:
      return account_manager::AccountAdditionSource::kGeminiInChromeReauth;
    default:
      NOTREACHED() << "Add account is requested from an unknown access point "
                   << static_cast<int>(access_point);
  }
}

account_manager::AccountAdditionSource GetAccountReauthSourceFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kSettingsAutofillAndPasswords:
      return account_manager::AccountAdditionSource::
          kChromeSettingsReauthAccountButton;
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
      return account_manager::AccountAdditionSource::
          kAvatarBubbleReauthAccountButton;
    case signin_metrics::AccessPoint::kExtensions:
      return account_manager::AccountAdditionSource::kChromeExtensionReauth;
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
      return account_manager::AccountAdditionSource::kChromeSyncPromoReauth;
    case signin_metrics::AccessPoint::kWebSignin:
      return account_manager::AccountAdditionSource::kContentAreaReauth;
    case signin_metrics::AccessPoint::kMenu:
      return account_manager::AccountAdditionSource::kChromeMenuTurnOnSync;
    case signin_metrics::AccessPoint::kGlicLaunchButton:
      return account_manager::AccountAdditionSource::kGeminiInChromeReauth;
    default:
      NOTREACHED() << "Reauth is requested from an unknown access point "
                   << static_cast<int>(access_point);
  }
}

}  // namespace signin_ui_util
