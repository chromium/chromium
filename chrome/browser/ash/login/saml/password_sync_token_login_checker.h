// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_LOGIN_CHECKER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_LOGIN_CHECKER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "components/account_id/account_id.h"
#include "net/base/backoff_entry.h"

namespace ash {

// Verifies local copy of the password sync token by executing API call. If
// token is invalid calls InSessionPasswordSyncManager to request online re-auth
// that will sync the password and update the token.
class PasswordSyncTokenLoginChecker
    : public PasswordSyncTokenFetcher::Consumer {
 public:
  // Observers of PasswordSyncTokenLoginChecker are notified of invalid sync
  // token.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInvalidSyncToken(const AccountId& account_id) = 0;
  };

  explicit PasswordSyncTokenLoginChecker(const AccountId& account_id,
                                         const std::string& sync_token,
                                         net::BackoffEntry* retry_backoff);
  ~PasswordSyncTokenLoginChecker() override;

  PasswordSyncTokenLoginChecker(const PasswordSyncTokenLoginChecker&) = delete;
  PasswordSyncTokenLoginChecker& operator=(
      const PasswordSyncTokenLoginChecker&) = delete;

  // Execute verification API call.
  void CheckForPasswordNotInSync();

  // Cancel all pending check requests.
  void CancelPendingChecks();

  // Returns true if pending check exists for account_id_.
  bool IsCheckPending();

  // Record start of token polling on the login screen.
  void RecordTokenPollingStart();

  // PasswordSyncTokenFetcher::Consumer
  void OnTokenCreated(const std::string& sync_token) override;
  void OnTokenFetched(const std::string& sync_token) override;
  void OnTokenVerified(bool is_valid) override;
  void OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType error_type) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class PasswordSyncTokenLoginCheckerTest;

  // Recheck after given |delay|.
  void RecheckAfter(base::TimeDelta delay);
  void NotifyObservers();

  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<PasswordSyncTokenFetcher> password_sync_token_fetcher_;
  const AccountId account_id_;
  const std::string sync_token_;
  raw_ptr<net::BackoffEntry> retry_backoff_ = nullptr;
  base::OneShotTimer recheck_timer_;

  base::WeakPtrFactory<PasswordSyncTokenLoginChecker> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_LOGIN_CHECKER_H_
