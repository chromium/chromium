// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cached_policy_key_loader_chromeos.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

namespace policy {

namespace {

// Path within |user_policy_key_dir_| that contains the policy key.
// "%s" must be substituted with the sanitized username.
const base::FilePath::CharType kPolicyKeyFile[] =
    FILE_PATH_LITERAL("%s/policy.pub");

// Maximum key size that will be loaded, in bytes.
const size_t kKeySizeLimit = 16 * 1024;

// Failures that can happen when loading the policy key,
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum class ValidationFailure {
  DBUS,
  LOAD_KEY,

  // Number of histogram buckets. Has to be the last element.
  MAX_VALUE,
};

void SampleValidationFailure(ValidationFailure sample) {
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserPolicyValidationFailure", sample,
                            ValidationFailure::MAX_VALUE);
}

}  // namespace

CachedPolicyKeyLoaderChromeOS::CachedPolicyKeyLoaderChromeOS(
    chromeos::CryptohomeClient* cryptohome_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const AccountId& account_id,
    const base::FilePath& user_policy_key_dir)
    : task_runner_(task_runner),
      cryptohome_client_(cryptohome_client),
      account_id_(account_id),
      user_policy_key_dir_(user_policy_key_dir) {}

CachedPolicyKeyLoaderChromeOS::~CachedPolicyKeyLoaderChromeOS() {}

void CachedPolicyKeyLoaderChromeOS::EnsurePolicyKeyLoaded(
    base::OnceClosure callback) {
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
  cryptohome_client_->GetSanitizedUsername(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
      base::BindOnce(&CachedPolicyKeyLoaderChromeOS::OnGetSanitizedUsername,
                     weak_factory_.GetWeakPtr()));
}

bool CachedPolicyKeyLoaderChromeOS::LoadPolicyKeyImmediately() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string sanitized_username =
      cryptohome_client_->BlockingGetSanitizedUsername(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id_));
  if (sanitized_username.empty())
    return false;

  cached_policy_key_path_ = user_policy_key_dir_.Append(
      base::StringPrintf(kPolicyKeyFile, sanitized_username.c_str()));
  cached_policy_key_ = LoadPolicyKey(cached_policy_key_path_);
  key_loaded_ = true;
  return true;
}

void CachedPolicyKeyLoaderChromeOS::ReloadPolicyKey(
    base::OnceClosure callback) {
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
    cryptohome_client_->GetSanitizedUsername(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
        base::BindOnce(&CachedPolicyKeyLoaderChromeOS::OnGetSanitizedUsername,
                       weak_factory_.GetWeakPtr()));
  } else {
    TriggerLoadPolicyKey();
  }
}

// static
std::string CachedPolicyKeyLoaderChromeOS::LoadPolicyKey(
    const base::FilePath& path) {
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

  if (key.empty())
    SampleValidationFailure(ValidationFailure::LOAD_KEY);

  return key;
}

void CachedPolicyKeyLoaderChromeOS::TriggerLoadPolicyKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&CachedPolicyKeyLoaderChromeOS::LoadPolicyKey,
                     cached_policy_key_path_),
      base::BindOnce(&CachedPolicyKeyLoaderChromeOS::OnPolicyKeyLoaded,
                     weak_factory_.GetWeakPtr()));
}

void CachedPolicyKeyLoaderChromeOS::OnPolicyKeyLoaded(const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cached_policy_key_ = key;
  key_loaded_ = true;
  key_load_in_progress_ = false;

  NotifyAndClearCallbacks();
}

void CachedPolicyKeyLoaderChromeOS::OnGetSanitizedUsername(
    base::Optional<std::string> sanitized_username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sanitized_username || sanitized_username->empty()) {
    SampleValidationFailure(ValidationFailure::DBUS);

    // Don't bother trying to load a key if we don't know where it is - just
    // signal that the load attempt has finished.
    key_load_in_progress_ = false;
    NotifyAndClearCallbacks();

    return;
  }

  cached_policy_key_path_ = user_policy_key_dir_.Append(
      base::StringPrintf(kPolicyKeyFile, sanitized_username->c_str()));
  TriggerLoadPolicyKey();
}

void CachedPolicyKeyLoaderChromeOS::NotifyAndClearCallbacks() {
  std::vector<base::OnceClosure> callbacks = std::move(key_loaded_callbacks_);
  key_loaded_callbacks_.clear();

  for (auto& callback : callbacks)
    std::move(callback).Run();
}

}  // namespace policy
