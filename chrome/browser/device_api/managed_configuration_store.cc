// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_store.h"

ManagedConfigurationStore::ManagedConfigurationStore(
    scoped_refptr<base::SequencedTaskRunner> backend_sequence,
    const url::Origin& origin,
    const base::FilePath& path)
    : backend_sequence_(backend_sequence),
      origin_(origin),
      path_(path),
      observers_(new base::ObserverListThreadSafe<
                 ManagedConfigurationAPI::Observer>()) {}

void ManagedConfigurationStore::InitializeOnBackend() {
  // LeveldbValueStore can only be initialized on a blocking sequnece.
  DCHECK(backend_sequence_->RunsTasksInCurrentSequence());
  store_ = std::make_unique<value_store::LeveldbValueStore>(
      "OriginManagedConfiguration", path_);
}

ManagedConfigurationStore::~ManagedConfigurationStore() {
  // Delete ValueStore on the FILE thread, since it can only operate on that
  // thread.
  backend_sequence_->DeleteSoon(FROM_HERE, std::move(store_));
}

void ManagedConfigurationStore::AddObserver(
    ManagedConfigurationAPI::Observer* observer) {
  observers_->AddObserver(observer);
}

void ManagedConfigurationStore::RemoveObserver(
    ManagedConfigurationAPI::Observer* observer) {
  observers_->RemoveObserver(observer);
}

void ManagedConfigurationStore::SetCurrentPolicy(
    const base::DictionaryValue& current_configuration) {
  DCHECK(backend_sequence_->RunsTasksInCurrentSequence());
  if (!store_)
    InitializeOnBackend();
  // Get the previous policies stored in the database.
  base::DictionaryValue previous_policy;
  value_store::ValueStore::ReadResult read_result = store_->Get();
  if (!read_result.status().ok()) {
    LOG(WARNING) << "Failed to read managed configuration for origin "
                 << origin_ << ": " << read_result.status().message;
    // Leave |previous_policy| empty, so that events are generated for every
    // policy in |current_policy|.
  } else {
    read_result.settings().Swap(&previous_policy);
  }

  std::vector<std::string> removed_keys;

  bool store_updated = false;
  for (auto kv : previous_policy.DictItems()) {
    if (!current_configuration.FindKey(kv.first))
      removed_keys.push_back(kv.first);
  }
  value_store::ValueStoreChangeList changes;
  value_store::LeveldbValueStore::WriteResult result =
      store_->Remove(removed_keys);

  if (result.status().ok()) {
    store_updated |= !result.changes().empty();
  }

  // IGNORE_QUOTA because these settings aren't writable by the origin, and
  // are configured by the device administrator.
  value_store::ValueStore::WriteOptions options =
      value_store::ValueStore::IGNORE_QUOTA;
  result = store_->Set(options, current_configuration);
  if (result.status().ok()) {
    store_updated |= !result.changes().empty();
  }

  if (!store_updated)
    return;
  observers_->Notify(
      FROM_HERE,
      &ManagedConfigurationAPI::Observer::OnManagedConfigurationChanged);
}

value_store::ValueStore::ReadResult ManagedConfigurationStore::Get(
    const std::vector<std::string>& keys) {
  DCHECK(backend_sequence_->RunsTasksInCurrentSequence());
  if (!store_)
    InitializeOnBackend();
  return store_->Get(keys);
}
