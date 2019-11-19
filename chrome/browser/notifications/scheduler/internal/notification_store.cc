// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_store.h"

#include "base/bind.h"
#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

namespace leveldb_proto {
void DataToProto(notifications::NotificationEntry* entry,
                 notifications::proto::NotificationEntry* proto) {
  NotificationEntryToProto(entry, proto);
}

void ProtoToData(notifications::proto::NotificationEntry* proto,
                 notifications::NotificationEntry* entry) {
  NotificationEntryFromProto(proto, entry);
}
}  // namespace leveldb_proto

namespace notifications {

NotificationStore::NotificationStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::NotificationEntry,
                                                 NotificationEntry>> db)
    : db_(std::move(db)) {}

NotificationStore::~NotificationStore() = default;

void NotificationStore::InitAndLoad(LoadCallback callback) {
  db_->Init(base::BindOnce(&NotificationStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void NotificationStore::OnDbInitialized(
    LoadCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, Entries());
    return;
  }

  // Load the data after a successful initialization.
  db_->LoadEntries(base::BindOnce(&NotificationStore::OnDataLoaded,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void NotificationStore::OnDataLoaded(
    LoadCallback callback,
    bool success,
    std::unique_ptr<EntryVector> entry_vector) {
  // The database failed to load.
  if (!success) {
    std::move(callback).Run(false, Entries());
    return;
  }

  // Success to load but no data.
  if (!entry_vector) {
    std::move(callback).Run(true, Entries());
    return;
  }

  // Load data.
  Entries entries;
  for (auto it = entry_vector->begin(); it != entry_vector->end(); ++it) {
    std::unique_ptr<NotificationEntry> notification_entry =
        std::make_unique<NotificationEntry>(std::move(*it));
    entries.emplace_back(std::move(notification_entry));
  }

  std::move(callback).Run(true, std::move(entries));
}

void NotificationStore::Add(const std::string& key,
                            const NotificationEntry& entry,
                            UpdateCallback callback) {
  Update(key, entry, std::move(callback));
}

void NotificationStore::Update(const std::string& key,
                               const NotificationEntry& entry,
                               UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  entries_to_save->emplace_back(std::make_pair(key, entry));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_remove*/,
                     std::move(callback));
}

void NotificationStore::Delete(const std::string& key,
                               UpdateCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  keys_to_delete->emplace_back(key);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*entries_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

}  // namespace notifications
