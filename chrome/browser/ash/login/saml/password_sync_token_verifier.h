// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/backoff_entry.h"

class Profile;

namespace user_manager {
class User;
}

namespace ash {

// Verifies local copy of the password sync token by executing API call. If
// token is invalid calls InSessionPasswordSyncManager to request online re-auth
// that will sync the password and update the token.
class PasswordSyncTokenVerifier : public KeyedService,
                                  public PasswordSyncTokenFetcher::Consumer {
 public:
  // Backoff policy for token fetch retry attempts in case token fetch failed or
  // returned invalid data.
  static const net::BackoffEntry::Policy kFetchTokenRetryBackoffPolicy;

  explicit PasswordSyncTokenVerifier(Profile* primary_profile);
  ~PasswordSyncTokenVerifier() override;

  PasswordSyncTokenVerifier(const PasswordSyncTokenVerifier&) = delete;
  PasswordSyncTokenVerifier& operator=(const PasswordSyncTokenVerifier&) =
      delete;

  // Execute verification API call.
  void CheckForPasswordNotInSync();

  // Fetch current sync token when starting a new session with online SAML.
  void FetchSyncTokenOnReauth();

  // Cancel all pending check requests.
  void CancelPendingChecks();

  // Record start of in-session token polling.
  void RecordTokenPollingStart();

  // PasswordSyncTokenFetcher::Consumer
  void OnTokenCreated(const std::string& sync_token) override;
  void OnTokenFetched(const std::string& sync_token) override;
  void OnTokenVerified(bool is_valid) override;
  void OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType error_type) override;

 private:
  // Recheck after given `delay`.
  void RecheckAfter(base::TimeDelta delay);
  // Init sync token.
  void CreateTokenAsync();

  const raw_ptr<Profile> primary_profile_;
  const raw_ptr<const user_manager::User> primary_user_;
  std::unique_ptr<PasswordSyncTokenFetcher> password_sync_token_fetcher_;
  net::BackoffEntry retry_backoff_;

  base::WeakPtrFactory<PasswordSyncTokenVerifier> weak_ptr_factory_{this};

  friend class PasswordSyncTokenVerifierTest;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_VERIFIER_H_
