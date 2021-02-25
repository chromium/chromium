// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"
#include "components/user_manager/user.h"
#include "net/base/backoff_entry.h"

namespace chromeos {

// Helper class to handle PasswordSyncTokenLoginChecker objects for all users
// on the login screen.
class PasswordSyncTokenCheckersCollection
    : PasswordSyncTokenLoginChecker::Observer {
 public:
  // Backoff policy for token fetch retry attempts in case token fetch failed or
  // returned invalid data.
  static const net::BackoffEntry::Policy kFetchTokenRetryBackoffPolicy;

  PasswordSyncTokenCheckersCollection();
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
  friend class PasswordSyncTokenLoginCheckerTest;
  friend class ExistingUserControllerForcedOnlineAuthTest;

  std::unordered_map<std::string,
                     std::unique_ptr<PasswordSyncTokenLoginChecker>>
      sync_token_checkers_;
  net::BackoffEntry sync_token_retry_backoff_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_CHECKERS_COLLECTION_H_
