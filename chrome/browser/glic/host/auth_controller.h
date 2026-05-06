// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_AUTH_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_AUTH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace glic {
class GlicCookieSynchronizer;

bool IsPrimaryAccountGoogleInternal(signin::IdentityManager& signin_manager);

// LINT.IfChange(CheckAuthBeforeLoadOutcome)
enum class CheckAuthBeforeLoadOutcome {
  // Test automation is enabled.
  kAutomationSkipped = 0,
  // User must manually sign in.
  kRequiresSignIn = 1,
  // GlicCookieSyncOnTokenChange is enabled and allowed skipping sync.
  kTokenChangeNoSyncNeeded = 2,
  // Skipped due to GlicSkipCookieSyncOnOpen.
  kSkipOnOpen = 3,
  // Cookie sync was attempted.
  kSyncAttempted = 4,
  kMaxValue = kSyncAttempted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicCheckAuthBeforeLoadOutcome)

// Decides when to refresh sign-in cookies for the webview.
class AuthController : public signin::IdentityManager::Observer {
 public:
  AuthController(Profile* profile, signin::IdentityManager* identity_manager);
  ~AuthController() override;

  // Called before the glic web client is loaded. Tries to sync cookies to the
  // webview partition, and then calls `callback`. Informs the caller whether
  // cookies could be synced.
  void CheckAuthBeforeLoad(
      base::OnceCallback<void(mojom::PrepareForClientResult)> callback);

  // Sync cookies, even if it appears as though a sync is not required.
  void ForceSyncCookies(base::OnceCallback<void(bool)> callback);

  // Called when the client reports that it has encountered an error.
  void OnClientError();

  // Called when the client reports that it has encountered a transient error.
  void OnClientTransientError(mojo_base::mojom::AbslStatusCode status_code);

  // Show the sign-in page. `after_signin` will be called after the user has
  // signed in. It will not be called if the user cancels the sign-in, or the
  // sign-in doesn't happen before:
  // * ShowReauthForAccount is called again
  // * The AuthController is destroyed
  // * Too much time has passed (5 minutes).
  // TODO(crbug.com/406529330): Track sign-in flow correctly.
  void ShowReauthForAccount(base::OnceClosure after_signin);

  void SetCookieSynchronizerForTesting(
      std::unique_ptr<GlicCookieSynchronizer> synchronizer);

  GlicCookieSynchronizer* GetCookieSynchronizerForTesting() const {
    return cookie_synchronizer_.get();
  }

  bool NeedsSyncForTesting() const;

  // signin::IdentityManager::Observer implementation.
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

 private:
  enum class TokenState {
    kUnknownError,
    kRequiresSignIn,
    kOk,
  };
  TokenState GetTokenState() const;
  base::WeakPtr<AuthController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void CookieSyncDone(base::OnceCallback<void(bool)> callback,
                      bool sync_success);
  void CookieSyncBeforeLoadDone(
      base::OnceCallback<void(mojom::PrepareForClientResult)> callback,
      bool sync_success);

  raw_ptr<Profile> profile_;
  base::OnceClosure after_signin_callback_;
  base::TimeTicks after_signin_callback_expiration_time_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<GlicCookieSynchronizer> cookie_synchronizer_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_;
  base::WeakPtrFactory<AuthController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_AUTH_CONTROLLER_H_
