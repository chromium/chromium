// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/backoff_entry.h"

namespace ash {

const net::BackoffEntry::Policy
    PasswordSyncTokenCheckersCollection::kFetchTokenRetryBackoffPolicy = {
        0,               // Number of initial errors to ignore.
        10 * 1000,       // Initial request delay in ms.
        2.0,             // Factor by which the waiting time will be multiplied.
        0.1,             // Fuzzing percentage.
        60 * 60 * 1000,  // Maximum request delay in ms.
        -1,              // Never discard the entry.
        true,            // Use initial delay in the first retry.
};

PasswordSyncTokenCheckersCollection::PasswordSyncTokenCheckersCollection()
    : sync_token_retry_backoff_(&kFetchTokenRetryBackoffPolicy) {}

PasswordSyncTokenCheckersCollection::~PasswordSyncTokenCheckersCollection() =
    default;

void PasswordSyncTokenCheckersCollection::StartPasswordSyncCheckers(
    const user_manager::UserList& users,
    PasswordSyncTokenLoginChecker::Observer* observer) {
  for (user_manager::User* user : users) {
    // Online login already enforced for the user - no further checks are
    // required.
    if (!user->using_saml() || user->force_online_signin())
      continue;

    user_manager::KnownUser known_user(g_browser_process->local_state());
    const std::string* sync_token =
        known_user.GetPasswordSyncToken(user->GetAccountId());
    if (sync_token && !sync_token->empty() &&
        !base::Contains(sync_token_checkers_, *sync_token)) {
      sync_token_checkers_.insert(
          {*sync_token,
           std::make_unique<PasswordSyncTokenLoginChecker>(
               user->GetAccountId(), *sync_token, &sync_token_retry_backoff_)});
      if (observer)
        sync_token_checkers_[*sync_token]->AddObserver(observer);
      sync_token_checkers_[*sync_token]->AddObserver(this);
      sync_token_checkers_[*sync_token]->RecordTokenPollingStart();
      sync_token_checkers_[*sync_token]->CheckForPasswordNotInSync();
    }
  }
}

void PasswordSyncTokenCheckersCollection::OnInvalidSyncToken(
    const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* sync_token = known_user.GetPasswordSyncToken(account_id);
  if (!sync_token)
    return;
  if (base::Contains(sync_token_checkers_, *sync_token))
    sync_token_checkers_.erase(*sync_token);
}

}  // namespace ash
