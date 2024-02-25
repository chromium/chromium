// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/cached_policy_key_loader.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"

namespace policy {

namespace {

// Path within |user_policy_key_dir_| that contains the policy key.
// "%s" must be substituted with the sanitized username.
const base::FilePath::CharType kPolicyKeyFile[] =
    FILE_PATH_LITERAL("%s/policy.pub");

// Maximum key size that will be loaded, in bytes.
const size_t kKeySizeLimit = 16 * 1024;

}  // namespace

CachedPolicyKeyLoader::CachedPolicyKeyLoader(
    ash::CryptohomeMiscClient* cryptohome_misc_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const AccountId& account_id,
    const base::FilePath& user_policy_key_dir)
    : task_runner_(task_runner),
      cryptohome_misc_client_(cryptohome_misc_client),
      account_id_(account_id),
      user_policy_key_dir_(user_policy_key_dir) {}

CachedPolicyKeyLoader::~CachedPolicyKeyLoader() {}

void CachedPolicyKeyLoader::EnsurePolicyKeyLoaded(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (key_loaded_) {
    std::move(callback).Run();
    return;
  }

  key_loaded_callbacks_.push_back(std::move(callback));

  // If a key load is in progress, the callback will be called once it finishes.
  // No need to trigger another one.
  if (key_load_in_progress_)
    return;

  key_load_in_progress_ = true;

  // Get the hashed username that's part of the key's path, to determine
  // |cached_policy_key_path_|.
  user_data_auth::GetSanitizedUsernameRequest request;
  request.set_username(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
          .account_id());
  cryptohome_misc_client_->GetSanitizedUsername(
      request, base::BindOnce(&CachedPolicyKeyLoader::OnGetSanitizedUsername,
                              weak_factory_.GetWeakPtr()));
}

bool CachedPolicyKeyLoader::LoadPolicyKeyImmediately() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  user_data_auth::GetSanitizedUsernameRequest request;
  request.set_username(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
          .account_id());
  std::optional<user_data_auth::GetSanitizedUsernameReply> reply =
      cryptohome_misc_client_->BlockingGetSanitizedUsername(request);
  if (!reply.has_value() || reply->sanitized_username().empty()) {
    return false;
  }

  cached_policy_key_path_ = user_policy_key_dir_.Append(
      base::StringPrintf(kPolicyKeyFile, reply->sanitized_username().c_str()));
  cached_policy_key_ = LoadPolicyKey(cached_policy_key_path_);
  key_loaded_ = true;
  return true;
}

void CachedPolicyKeyLoader::ReloadPolicyKey(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  key_loaded_callbacks_.push_back(std::move(callback));

  if (key_load_in_progress_) {
    // When a load is in progress, cancel the current load by invalidating weak
    // pointers and before starting a new load.
    weak_factory_.InvalidateWeakPtrs();
  }

  key_load_in_progress_ = true;

  if (cached_policy_key_path_.empty()) {
    // Get the hashed username that's part of the key's path, to determine
    // |cached_policy_key_path_|.
    user_data_auth::GetSanitizedUsernameRequest request;
    request.set_username(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
            .account_id());
    cryptohome_misc_client_->GetSanitizedUsername(
        request, base::BindOnce(&CachedPolicyKeyLoader::OnGetSanitizedUsername,
                                weak_factory_.GetWeakPtr()));
  } else {
    TriggerLoadPolicyKey();
  }
}

// static
std::string CachedPolicyKeyLoader::LoadPolicyKey(const base::FilePath& path) {
  std::string key;

  if (!base::PathExists(path)) {
    // There is no policy key the first time that a user fetches policy. If
    // |path| does not exist then that is the most likely scenario, so there's
    // no need to sample a failure.
    VLOG(1) << "No key at " << path.value();
    return key;
  }

  const bool read_success =
      base::ReadFileToStringWithMaxSize(path, &key, kKeySizeLimit);
  // If the read was successful and the file size is 0 or if the read fails
  // due to file size exceeding |kKeySizeLimit|, log error.
  if ((read_success && key.length() == 0) ||
      (!read_success && key.length() == kKeySizeLimit)) {
    LOG(ERROR) << "Key at " << path.value()
               << (read_success ? " is empty." : " exceeds size limit");
    key.clear();
  } else if (!read_success) {
    LOG(ERROR) << "Failed to read key at " << path.value();
  }

  return key;
}

void CachedPolicyKeyLoader::TriggerLoadPolicyKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CachedPolicyKeyLoader::LoadPolicyKey,
                     cached_policy_key_path_),
      base::BindOnce(&CachedPolicyKeyLoader::OnPolicyKeyLoaded,
                     weak_factory_.GetWeakPtr()));
}

void CachedPolicyKeyLoader::OnPolicyKeyLoaded(const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cached_policy_key_ = key;
  key_loaded_ = true;
  key_load_in_progress_ = false;

  NotifyAndClearCallbacks();
}

void CachedPolicyKeyLoader::OnGetSanitizedUsername(
    std::optional<user_data_auth::GetSanitizedUsernameReply> reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reply.has_value() || reply->sanitized_username().empty()) {
    // Don't bother trying to load a key if we don't know where it is - just
    // signal that the load attempt has finished.
    key_load_in_progress_ = false;
    NotifyAndClearCallbacks();

    return;
  }

  cached_policy_key_path_ = user_policy_key_dir_.Append(
      base::StringPrintf(kPolicyKeyFile, reply->sanitized_username().c_str()));
  TriggerLoadPolicyKey();
}

void CachedPolicyKeyLoader::NotifyAndClearCallbacks() {
  std::vector<base::OnceClosure> callbacks = std::move(key_loaded_callbacks_);
  key_loaded_callbacks_.clear();

  for (auto& callback : callbacks)
    std::move(callback).Run();
}

}  // namespace policy
