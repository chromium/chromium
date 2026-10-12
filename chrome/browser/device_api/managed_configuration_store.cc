// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_store.h"

#include <utility>
#include <variant>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/value_store_change.h"

ManagedConfigurationStore::ManagedConfigurationStore(const url::Origin& origin,
                                                     const base::FilePath& path)
    : origin_(origin), path_(path) {}

ManagedConfigurationStore::~ManagedConfigurationStore() = default;

void ManagedConfigurationStore::Initialize() {
  // LeveldbValueStore can only be initialized on a blocking sequnece.
  store_ = std::make_unique<value_store::LeveldbValueStore>(
      "OriginManagedConfiguration", path_);
}

base::expected<bool, std::monostate>
ManagedConfigurationStore::SetCurrentPolicy(
    const base::DictValue& current_configuration) {
  if (!store_) {
    Initialize();
  }
  // Get the previous policies stored in the database.
  value_store::ValueStore::ReadResult read_result = store_->Get();
  if (!read_result.status().ok()) {
    LOG(WARNING) << "Failed to read managed configuration for origin "
                 << origin_ << ": " << read_result.status().message;
    return base::unexpected(std::monostate());
  }
  base::DictValue previous_policy = std::move(read_result.settings());

  std::vector<std::string> removed_keys;

  bool store_updated = false;
  for (auto item : previous_policy) {
    if (!current_configuration.Find(item.first)) {
      removed_keys.push_back(item.first);
    }
  }
  value_store::ValueStoreChangeList changes;
  value_store::LeveldbValueStore::WriteResult result =
      store_->Remove(removed_keys);

  if (!result.status().ok()) {
    return base::unexpected(std::monostate());
  }
  store_updated |= !result.changes().empty();

  // IGNORE_QUOTA because these settings aren't writable by the origin, and
  // are configured by the device administrator.
  value_store::ValueStore::WriteOptions options =
      value_store::ValueStore::IGNORE_QUOTA;
  result = store_->Set(options, current_configuration);
  if (!result.status().ok()) {
    return base::unexpected(std::monostate());
  }
  store_updated |= !result.changes().empty();

  return base::ok(store_updated);
}

std::optional<base::DictValue> ManagedConfigurationStore::Get(
    const std::vector<std::string>& keys) {
  if (!store_) {
    Initialize();
  }

  auto result = store_->Get(keys);

  if (!result.status().ok()) {
    return std::nullopt;
  }

  return result.PassSettings();
}

std::optional<base::DictValue> ManagedConfigurationStore::GetAll() {
  if (!store_) {
    Initialize();
  }

  auto result = store_->Get();

  if (!result.status().ok()) {
    return std::nullopt;
  }

  return result.PassSettings();
}
