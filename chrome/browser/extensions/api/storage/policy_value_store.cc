// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/policy_value_store.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/value_store/value_store_change.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/common/extension_id.h"

using value_store::ValueStore;

namespace extensions {

namespace {

ValueStore::Status ReadOnlyError() {
  return ValueStore::Status(ValueStore::READ_ONLY,
                            "This is a read-only store.");
}

}  // namespace

PolicyValueStore::PolicyValueStore(
    const ExtensionId& extension_id,
    SequenceBoundSettingsChangedCallback observer,
    std::unique_ptr<ValueStore> delegate)
    : extension_id_(extension_id),
      observer_(std::move(observer)),
      delegate_(std::move(delegate)) {}

PolicyValueStore::~PolicyValueStore() {}

void PolicyValueStore::SetCurrentPolicy(const policy::PolicyMap& policy) {
  DCHECK(IsOnBackendSequence());
  // Convert |policy| to a dictionary value. Only include mandatory policies
  // for now.
  base::Value::Dict current_policy;
  for (const auto& it : policy) {
    if (it.second.level == policy::POLICY_LEVEL_MANDATORY) {
      current_policy.Set(it.first, it.second.value_unsafe()->Clone());
    }
  }

  // Get the previous policies stored in the database.
  // TODO(joaodasilva): it'd be better to have a less expensive way of
  // determining which keys are currently stored, or of determining which keys
  // must be removed.
  base::Value::Dict previous_policy;
  ValueStore::ReadResult read_result = delegate_->Get();

  if (!read_result.status().ok()) {
    LOG(WARNING) << "Failed to read managed settings for extension "
                 << extension_id_ << ": " << read_result.status().message;
    // Leave |previous_policy| empty, so that events are generated for every
    // policy in |current_policy|.
  } else {
    std::swap(read_result.settings(), previous_policy);
  }

  // Now get two lists of changes: changes after setting the current policies,
  // and changes after removing old policies that aren't in |current_policy|
  // anymore.
  std::vector<std::string> removed_keys;
  for (auto kv : previous_policy) {
    if (!current_policy.Find(kv.first))
      removed_keys.push_back(kv.first);
  }

  value_store::ValueStoreChangeList changes;

  {
    WriteResult result = delegate_->Remove(removed_keys);
    if (result.status().ok()) {
      auto new_changes = result.PassChanges();
      changes.insert(changes.end(),
                     std::make_move_iterator(new_changes.begin()),
                     std::make_move_iterator(new_changes.end()));
    }
  }

  {
    // IGNORE_QUOTA because these settings aren't writable by the extension, and
    // are configured by the domain administrator.
    ValueStore::WriteOptions options = ValueStore::IGNORE_QUOTA;
    WriteResult result = delegate_->Set(options, current_policy);
    if (result.status().ok()) {
      auto new_changes = result.PassChanges();
      changes.insert(changes.end(),
                     std::make_move_iterator(new_changes.begin()),
                     std::make_move_iterator(new_changes.end()));
    }
  }

  if (!changes.empty()) {
    observer_->Run(extension_id_, StorageAreaNamespace::kManaged,
                   /*session_access_level=*/std::nullopt,
                   value_store::ValueStoreChange::ToValue(std::move(changes)));
  }
}

void PolicyValueStore::DeleteStorage() {
  // This is called from our owner, indicating that storage for this extension
  // should be removed.
  delegate_->Clear();
}

size_t PolicyValueStore::GetBytesInUse(const std::string& key) {
  // LeveldbValueStore doesn't implement this; and the underlying database
  // isn't acccessible to the extension in any case; from the extension's
  // perspective this is a read-only store.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // See note above.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse() {
  // See note above.
  return 0;
}

ValueStore::ReadResult PolicyValueStore::Get(const std::string& key) {
  return delegate_->Get(key);
}

ValueStore::ReadResult PolicyValueStore::Get(
    const std::vector<std::string>& keys) {
  return delegate_->Get(keys);
}

ValueStore::ReadResult PolicyValueStore::Get() {
  return delegate_->Get();
}

ValueStore::WriteResult PolicyValueStore::Set(WriteOptions options,
                                              const std::string& key,
                                              const base::Value& value) {
  return WriteResult(ReadOnlyError());
}

ValueStore::WriteResult PolicyValueStore::Set(
    WriteOptions options,
    const base::Value::Dict& settings) {
  return WriteResult(ReadOnlyError());
}

ValueStore::WriteResult PolicyValueStore::Remove(const std::string& key) {
  return WriteResult(ReadOnlyError());
}

ValueStore::WriteResult PolicyValueStore::Remove(
    const std::vector<std::string>& keys) {
  return WriteResult(ReadOnlyError());
}

ValueStore::WriteResult PolicyValueStore::Clear() {
  return WriteResult(ReadOnlyError());
}

}  // namespace extensions
