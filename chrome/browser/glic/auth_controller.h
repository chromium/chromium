// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_AUTH_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_AUTH_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace glic {
class GlicCookieSynchronizer;

// Decides when to refresh sign-in cookies for the webview.
class AuthController : public signin::IdentityManager::Observer {
 public:
  // Result of `BeforeShow()`.
  enum BeforeShowResult {
    // The glic webview should have valid sign-in cookies.
    kReady = 0,
    // Sign-in cookies cannot be automatically refreshed. A new tab has been
    // opened to allow the user to sign-in manually.
    kShowingReauthSigninPage = 1,
    // Syncing cookies failed.
    kSyncFailed = 2,
  };

  AuthController(Profile* profile,
                 signin::IdentityManager* identity_manager,
                 bool use_for_fre);
  ~AuthController() override;

  // Called before the glic web client is loaded. Tries to sync cookies to the
  // webview partition, and then calls `callback`. Informs the caller whether
  // cookies could be synced.
  void CheckAuthBeforeLoad(base::OnceCallback<void(bool)> callback);

  // Called before the glic window is shown. Checks status of sign-in state and
  // webview cookies. See `BeforeShowResult` for result detail.
  void CheckAuthBeforeShow(base::OnceCallback<void(BeforeShowResult)> callback);

  // Sync cookies, even if it appears as though a sync is not required.
  void ForceSyncCookies(base::OnceCallback<void(bool)> callback);

  void SetCookieSynchronizerForTesting(
      std::unique_ptr<GlicCookieSynchronizer> synchronizer);

  GlicCookieSynchronizer* GetCookieSynchronizerForTesting() const {
    return cookie_synchronizer_.get();
  }

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
  base::WeakPtr<AuthController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void SyncCookiesIfRequired(base::OnceCallback<void(bool)> callback);
  void CookieSyncDone(base::OnceCallback<void(bool)> callback,
                      bool sync_success);

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::optional<base::TimeTicks> last_cookie_sync_time_;
  std::unique_ptr<GlicCookieSynchronizer> cookie_synchronizer_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_;
  base::WeakPtrFactory<AuthController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_AUTH_CONTROLLER_H_
