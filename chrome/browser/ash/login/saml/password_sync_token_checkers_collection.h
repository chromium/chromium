// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "net/base/backoff_entry.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Helper class to handle PasswordSyncTokenLoginChecker objects for all users
// on the login screen.
class PasswordSyncTokenCheckersCollection
    : PasswordSyncTokenLoginChecker::Observer {
 public:
  // Backoff policy for token fetch retry attempts in case token fetch failed or
  // returned invalid data.
  static const net::BackoffEntry::Policy kFetchTokenRetryBackoffPolicy;

  // `local_state` must be non-null and must outlive `this`.
  // `shared_url_loader_factory` must be non-null.
  PasswordSyncTokenCheckersCollection(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  ~PasswordSyncTokenCheckersCollection() override;

  PasswordSyncTokenCheckersCollection(
      const PasswordSyncTokenCheckersCollection&) = delete;
  PasswordSyncTokenCheckersCollection& operator=(
      const PasswordSyncTokenCheckersCollection&) = delete;

  void StartPasswordSyncCheckers(
      const user_manager::UserList& users,
      PasswordSyncTokenLoginChecker::Observer* observer);

  // PasswordSyncTokenLoginChecker::Observer
  void OnInvalidSyncToken(const AccountId& account_id) override;

 private:
  friend class ExistingUserControllerForcedOnlineAuthTest;
  friend class PasswordSyncTokenLoginCheckerTest;

  const raw_ref<PrefService> local_state_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

  std::unordered_map<std::string,
                     std::unique_ptr<PasswordSyncTokenLoginChecker>>
      sync_token_checkers_;
  net::BackoffEntry sync_token_retry_backoff_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_
