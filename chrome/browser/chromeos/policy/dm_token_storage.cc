// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dm_token_storage.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::string EncryptToken(const std::string& system_salt,
                         const std::string& dm_token) {
  chromeos::CryptohomeTokenEncryptor encryptor(system_salt);
  return encryptor.EncryptWithSystemSalt(dm_token);
}

std::string DecryptToken(const std::string& system_salt,
                         const std::string encrypted_dm_token) {
  chromeos::CryptohomeTokenEncryptor encryptor(system_salt);
  return encryptor.DecryptWithSystemSalt(encrypted_dm_token);
}

}  // namespace

namespace policy {

DMTokenStorage::DMTokenStorage(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &DMTokenStorage::OnSystemSaltRecevied, weak_ptr_factory_.GetWeakPtr()));
}

DMTokenStorage::~DMTokenStorage() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FlushStoreTokenCallback(false);
  FlushRetrieveTokenCallback(std::string());
}

// static
void DMTokenStorage::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceDMToken, std::string());
}

void DMTokenStorage::StoreDMToken(const std::string& dm_token,
                                  StoreCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!store_callback_.is_null()) {
    DLOG(ERROR)
        << "Failed to store DM token: Previous store operation is not finished";
    std::move(callback).Run(false);
    return;
  }
  if (!retrieve_callbacks_.empty()) {
    DLOG(ERROR)
        << "Failed to store DM token: Retrieve operation is not finished";
    std::move(callback).Run(false);
    return;
  }
  store_callback_ = std::move(callback);
  dm_token_ = dm_token;
  switch (state_) {
    case SaltState::LOADING:
      // Do nothing. Waiting for system salt.
      break;
    case SaltState::LOADED:
      EncryptAndStoreToken();
      break;
    case SaltState::ERROR:
      FlushStoreTokenCallback(false);
      break;
  }
}

void DMTokenStorage::RetrieveDMToken(RetrieveCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!store_callback_.is_null()) {
    DCHECK(retrieve_callbacks_.empty());
    DLOG(ERROR)
        << "Failed to retrieve DM token: Store operation is not finished";
    std::move(callback).Run(std::string());
    return;
  }
  retrieve_callbacks_.push_back(std::move(callback));
  switch (state_) {
    case SaltState::LOADING:
      // Do nothing. Waiting for system salt.
      break;
    case SaltState::LOADED:
      if (retrieve_callbacks_.size() == 1) {  // First consumer.
        LoadAndDecryptToken();
      }
      break;
    case SaltState::ERROR:
      FlushRetrieveTokenCallback(std::string());
      break;
  }
}

void DMTokenStorage::OnSystemSaltRecevied(const std::string& system_salt) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  system_salt_ = system_salt;
  if (system_salt_.empty()) {
    state_ = SaltState::ERROR;
    DLOG(ERROR) << "Failed to get system salt.";
    FlushStoreTokenCallback(false);
    FlushRetrieveTokenCallback(std::string());
    return;
  }
  // Should not be concurrent store and get operations.
  DCHECK(store_callback_.is_null() || retrieve_callbacks_.empty());
  state_ = SaltState::LOADED;
  if (!store_callback_.is_null())
    EncryptAndStoreToken();
  else if (!retrieve_callbacks_.empty())
    LoadAndDecryptToken();
}

void DMTokenStorage::EncryptAndStoreToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!system_salt_.empty());
  DCHECK(!dm_token_.empty());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&EncryptToken, system_salt_, dm_token_),
      base::BindOnce(&DMTokenStorage::OnTokenEncrypted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DMTokenStorage::OnTokenEncrypted(const std::string& encrypted_dm_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (encrypted_dm_token.empty()) {
    DLOG(ERROR) << "Failed to encrypt DM token.";
  } else {
    local_state_->SetString(prefs::kDeviceDMToken, encrypted_dm_token);
  }
  FlushStoreTokenCallback(!encrypted_dm_token.empty());
}

void DMTokenStorage::LoadAndDecryptToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(SaltState::LOADED, state_);
  std::string encrypted_dm_token =
      local_state_->GetString(prefs::kDeviceDMToken);
  if (!encrypted_dm_token.empty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DecryptToken, system_salt_, encrypted_dm_token),
        base::BindOnce(&DMTokenStorage::FlushRetrieveTokenCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    DLOG(ERROR) << "No DM token in the local state.";
    FlushRetrieveTokenCallback(std::string());
  }
}

void DMTokenStorage::FlushStoreTokenCallback(bool status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!store_callback_.is_null()) {
    std::move(store_callback_).Run(status);
  }
}

void DMTokenStorage::FlushRetrieveTokenCallback(const std::string& dm_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (retrieve_callbacks_.empty())
    return;
  if (dm_token.empty())
    DLOG(ERROR) << "Failed to retrieve DM token.";
  std::vector<RetrieveCallback> callbacks;
  callbacks.swap(retrieve_callbacks_);
  for (RetrieveCallback& callback : callbacks)
    std::move(callback).Run(dm_token);
}

}  // namespace policy
