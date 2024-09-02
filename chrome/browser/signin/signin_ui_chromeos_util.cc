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
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_ORGANIZATION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoAddAccount;
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeMenuTurnOnSync;
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSigninPromoAddAccount;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Add account is requested from an unknown access point "
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
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_ORGANIZATION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeSyncPromoReauth;
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kContentAreaReauth;
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kChromeMenuTurnOnSync;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Reauth is requested from an unknown access point "
          << static_cast<int>(access_point);
      return account_manager::AccountManagerFacade::AccountAdditionSource::
          kMaxValue;
  }
}

}  // namespace signin_ui_util
