// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"

class PrefService;

namespace user_manager {
class User;
}

namespace ash {

// Manages SAML password sync for multiple customer devices.
class InSessionPasswordSyncManager : public PasswordSyncTokenFetcher::Consumer {
 public:
  // `local_state` must be non-null and must outlive `this`.
  InSessionPasswordSyncManager(PrefService* local_state,
                               Profile* primary_profile);
  ~InSessionPasswordSyncManager() override;

  InSessionPasswordSyncManager(const InSessionPasswordSyncManager&) = delete;
  InSessionPasswordSyncManager& operator=(const InSessionPasswordSyncManager&) =
      delete;

  // PasswordSyncTokenFetcher::Consumer
  void OnTokenCreated(const std::string& sync_token) override;
  void OnTokenFetched(const std::string& sync_token) override;
  void OnTokenVerified(bool is_valid) override;
  void OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType error_type) override;

  // Password sync token API calls.
  void FetchTokenAsync();
  void CreateTokenAsync();

 private:
  void ResetReauthRequiredBySamlTokenDismatch();

  const raw_ref<PrefService> local_state_;
  const raw_ptr<Profile> primary_profile_;
  const raw_ptr<const user_manager::User, DanglingUntriaged> primary_user_;
  std::unique_ptr<PasswordSyncTokenFetcher> password_sync_token_fetcher_;

  friend class InSessionPasswordSyncManagerTest;
  friend class InSessionPasswordSyncManagerFactory;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_
