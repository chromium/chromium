// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

const base::TimeDelta kPollingInterval = base::Minutes(5);
}

PasswordSyncTokenLoginChecker::PasswordSyncTokenLoginChecker(
    const AccountId& account_id,
    const std::string& sync_token,
    net::BackoffEntry* retry_backoff)
    : account_id_(account_id),
      sync_token_(sync_token),
      retry_backoff_(retry_backoff) {
  DCHECK(!sync_token_.empty());
}

PasswordSyncTokenLoginChecker::~PasswordSyncTokenLoginChecker() = default;

void PasswordSyncTokenLoginChecker::RecheckAfter(base::TimeDelta delay) {
  CancelPendingChecks();
  recheck_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&PasswordSyncTokenLoginChecker::CheckForPasswordNotInSync,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordSyncTokenLoginChecker::CheckForPasswordNotInSync() {
  DCHECK(!password_sync_token_fetcher_);
  SystemNetworkContextManager* network_context_manager =
      g_browser_process->system_network_context_manager();
  if (!network_context_manager)
    return;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      network_context_manager->GetSharedURLLoaderFactory();
  if (!url_loader_factory.get())
    return;

  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      url_loader_factory, /*primary_profile_ = */ nullptr, this);
  password_sync_token_fetcher_->StartTokenVerify(sync_token_);
}

void PasswordSyncTokenLoginChecker::CancelPendingChecks() {
  // We should not have any active request at this point. DCHECK makes sure it
  // is really the case for the dev build. In a release build InvalidateWeakPtrs
  // helps to recover by cancelling potential existing requests.
  DCHECK(!IsCheckPending());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool PasswordSyncTokenLoginChecker::IsCheckPending() {
  return recheck_timer_.IsRunning();
}

void PasswordSyncTokenLoginChecker::RecordTokenPollingStart() {
  RecordStartOfSyncTokenPollingUMA(/*in_session=*/false);
}

void PasswordSyncTokenLoginChecker::OnTokenCreated(
    const std::string& sync_token) {
  /* ignored */
}

void PasswordSyncTokenLoginChecker::OnTokenFetched(
    const std::string& sync_token) {
  /* ignored */
}

void PasswordSyncTokenLoginChecker::OnTokenVerified(bool is_valid) {
  retry_backoff_->InformOfRequest(true);
  password_sync_token_fetcher_.reset();
  if (is_valid) {
    // Schedule next token check after base interval.
    RecheckAfter(kPollingInterval);
    return;
  }

  // Set force_online flag and refresh the login screen.
  user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id_, true);
  NotifyObservers();
}

void PasswordSyncTokenLoginChecker::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  retry_backoff_->InformOfRequest(false);
  password_sync_token_fetcher_.reset();
  // Schedule next token check with interval calculated with exponential
  // backoff.
  RecheckAfter(retry_backoff_->GetTimeUntilRelease());
}

void PasswordSyncTokenLoginChecker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PasswordSyncTokenLoginChecker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PasswordSyncTokenLoginChecker::NotifyObservers() {
  for (auto& observer : observer_list_) {
    observer.OnInvalidSyncToken(account_id_);
  }
}

}  // namespace ash
