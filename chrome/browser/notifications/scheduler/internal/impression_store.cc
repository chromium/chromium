// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_store.h"

#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

namespace leveldb_proto {

void DataToProto(notifications::ClientState* client_state,
                 notifications::proto::ClientState* proto) {
  ClientStateToProto(client_state, proto);
}

void ProtoToData(notifications::proto::ClientState* proto,
                 notifications::ClientState* client_state) {
  ClientStateFromProto(proto, client_state);
}

}  // namespace leveldb_proto

namespace notifications {

ImpressionStore::ImpressionStore(
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<proto::ClientState, ClientState>> db)
    : db_(std::move(db)) {}

ImpressionStore::~ImpressionStore() = default;

void ImpressionStore::InitAndLoad(LoadCallback callback) {
  db_->Init(base::BindOnce(&ImpressionStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void ImpressionStore::OnDbInitialized(LoadCallback callback,
                                      leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, Entries());
    return;
  }

  // Load the data after a successful initialization.
  db_->LoadEntries(base::BindOnce(&ImpressionStore::OnDataLoaded,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void ImpressionStore::OnDataLoaded(LoadCallback callback,
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
    std::unique_ptr<ClientState> client_state = std::make_unique<ClientState>();
    *client_state = std::move(*it);
    entries.emplace_back(std::move(client_state));
  }

  std::move(callback).Run(true, std::move(entries));
}

void ImpressionStore::Add(const std::string& key,
                          const ClientState& client_state,
                          UpdateCallback callback) {
  Update(key, client_state, std::move(callback));
}

void ImpressionStore::Update(const std::string& key,
                             const ClientState& client_state,
                             UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  entries_to_save->emplace_back(std::make_pair(key, client_state));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_remove*/,
                     std::move(callback));
}

void ImpressionStore::Delete(const std::string& key, UpdateCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  keys_to_delete->emplace_back(key);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*entries_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

}  // namespace notifications
