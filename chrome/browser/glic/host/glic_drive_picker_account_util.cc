// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drive_picker_account_util.h"

#include <algorithm>
#include <utility>

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace glic {

namespace {

std::optional<DrivePickerAccountError> CheckCookieJar(
    const GaiaId& expected_gaia,
    const signin::AccountsInCookieJarInfo& cookie_info) {
  // The Google Picker API loads files with the profile's Google cookies. Losing
  // the session while Glic is interactive should generally not be possible
  // because AuthController transitions Glic to its sign-in/verify error UI when
  // credentials break; this check is defense-in-depth for cases where web
  // cookies are cleared in another tab (or diverge from a legacy <webview>
  // partition) before Glic reloads.
  if (cookie_info.AreAccountsFresh()) {
    const auto& signed_in = cookie_info.GetValidSignedInAccounts();
    const bool primary_in_cookies =
        std::ranges::any_of(signed_in, [&](const gaia::ListedAccount& a) {
          return a.gaia_id == expected_gaia;
        });
    if (!primary_in_cookies) {
      return DrivePickerAccountError::kWebSessionSignedOut;
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<DrivePickerAccountError> CheckDrivePickerPrimaryAccount(
    signin::IdentityManager* identity_manager,
    const GaiaId& expected_gaia) {
  if (!identity_manager) {
    return DrivePickerAccountError::kChromeSignInRequired;
  }
  const CoreAccountInfo primary =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary.IsEmpty() || primary.gaia.empty()) {
    return DrivePickerAccountError::kChromeSignInRequired;
  }
  // The web client must be signed in as Chrome's primary account, since the
  // picker's OAuth token is minted for the primary account.
  if (expected_gaia.empty() || expected_gaia != primary.gaia) {
    return DrivePickerAccountError::kAccountMismatch;
  }
  // Without a valid refresh token the picker's token fetch would fail.
  if (!identity_manager->HasAccountWithRefreshToken(primary.account_id) ||
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary.account_id)) {
    return DrivePickerAccountError::kChromeSignInRequired;
  }
  return std::nullopt;
}

std::optional<DrivePickerAccountError> CheckDrivePickerAccount(
    signin::IdentityManager* identity_manager,
    const GaiaId& expected_gaia) {
  if (std::optional<DrivePickerAccountError> error =
          CheckDrivePickerPrimaryAccount(identity_manager, expected_gaia)) {
    return error;
  }
  return CheckCookieJar(expected_gaia,
                        identity_manager->GetAccountsInCookieJar());
}

DrivePickerAccountWatcher::DrivePickerAccountWatcher(
    signin::IdentityManager* identity_manager,
    GaiaId expected_gaia,
    AccountInvalidatedCallback on_account_invalidated)
    : expected_gaia_(std::move(expected_gaia)),
      on_account_invalidated_(std::move(on_account_invalidated)) {
  if (identity_manager) {
    observation_.Observe(identity_manager);
  }
}

DrivePickerAccountWatcher::~DrivePickerAccountWatcher() = default;

void DrivePickerAccountWatcher::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  EvaluatePrimaryAccountAndToken();
}

void DrivePickerAccountWatcher::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  EvaluatePrimaryAccountAndToken();
}

void DrivePickerAccountWatcher::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  EvaluatePrimaryAccountAndToken();
}

void DrivePickerAccountWatcher::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  // A failed /ListAccounts fetch says nothing about the session; ignore it.
  if (error.state() != GoogleServiceAuthError::NONE) {
    return;
  }
  if (std::optional<DrivePickerAccountError> reason =
          CheckDrivePickerPrimaryAccount(observation_.GetSource(),
                                         expected_gaia_)) {
    Report(*reason);
    return;
  }
  if (std::optional<DrivePickerAccountError> reason =
          CheckCookieJar(expected_gaia_, accounts_in_cookie_jar_info)) {
    Report(*reason);
  }
}

void DrivePickerAccountWatcher::OnAccountsCookieDeletedByUserAction() {
  Report(DrivePickerAccountError::kWebSessionSignedOut);
}

void DrivePickerAccountWatcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  observation_.Reset();
}

void DrivePickerAccountWatcher::EvaluatePrimaryAccountAndToken() {
  if (std::optional<DrivePickerAccountError> reason =
          CheckDrivePickerPrimaryAccount(observation_.GetSource(),
                                         expected_gaia_)) {
    Report(*reason);
  }
}

void DrivePickerAccountWatcher::Report(DrivePickerAccountError reason) {
  if (on_account_invalidated_) {
    observation_.Reset();
    std::move(on_account_invalidated_).Run(reason);
  }
}

}  // namespace glic
