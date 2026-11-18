// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_ACCOUNT_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_ACCOUNT_UTIL_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_id.h"

namespace glic {

// Why Chrome's Drive picker cannot be used with the current profile account
// state.
enum class DrivePickerAccountError {
  // Chrome has no signed-in primary account, or the primary account is missing
  // a valid refresh token (e.g. sign-in paused).
  kChromeSignInRequired,
  // The expected Gaia ID is empty or does not match the primary account.
  kAccountMismatch,
  // The primary account is absent from or signed out of the profile's Google
  // cookie jar.
  kWebSessionSignedOut,
};

// Returns why Chrome's Drive picker must not be shown (or must be closed) for
// the web client signed in as `expected_gaia`, or nullopt if it may be shown.
// The picker authenticates with the primary account's OAuth token, and the
// Google Picker API loads files with the profile's Google cookies, so both the
// primary account's refresh token and its web session must be healthy, and the
// web client must be signed in to the same account.
//
// Fails with `kAccountMismatch` if `expected_gaia` is empty or does not match
// the primary account's Gaia ID. Checks `identity_manager`'s current Gaia
// cookie jar state when it is fresh.
std::optional<DrivePickerAccountError> CheckDrivePickerAccount(
    signin::IdentityManager* identity_manager,
    const GaiaId& expected_gaia);

// While alive, watches for the primary account losing its web session or
// refresh token, or changing, and runs `on_account_invalidated` once with the
// reason.
class DrivePickerAccountWatcher : public signin::IdentityManager::Observer {
 public:
  using AccountInvalidatedCallback =
      base::OnceCallback<void(DrivePickerAccountError reason)>;

  // `expected_gaia` is the primary account's Gaia ID when the picker opened.
  DrivePickerAccountWatcher(signin::IdentityManager* identity_manager,
                            GaiaId expected_gaia,
                            AccountInvalidatedCallback on_account_invalidated);
  DrivePickerAccountWatcher(const DrivePickerAccountWatcher&) = delete;
  DrivePickerAccountWatcher& operator=(const DrivePickerAccountWatcher&) =
      delete;
  ~DrivePickerAccountWatcher() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  void EvaluatePrimaryAccountAndToken();
  void Report(DrivePickerAccountError reason);

  GaiaId expected_gaia_;
  AccountInvalidatedCallback on_account_invalidated_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_ACCOUNT_UTIL_H_
