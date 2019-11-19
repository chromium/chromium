// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/proto/client_state.pb.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "components/leveldb_proto/public/proto_database.h"

// Forward declaration for proto conversion.
namespace leveldb_proto {
void DataToProto(notifications::ClientState* client_state,
                 notifications::proto::ClientState* proto);

void ProtoToData(notifications::proto::ClientState* proto,
                 notifications::ClientState* client_state);
}  // namespace leveldb_proto

namespace notifications {

// An impression storage using a proto database to persist data.
class ImpressionStore : public CollectionStore<ClientState> {
 public:
  ImpressionStore(
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<proto::ClientState, ClientState>> db);
  ~ImpressionStore() override;

 private:
  using KeyEntryVector = std::vector<std::pair<std::string, ClientState>>;
  using KeyVector = std::vector<std::string>;
  using EntryVector = std::vector<ClientState>;

  // CollectionStore implementation.
  void InitAndLoad(LoadCallback callback) override;
  void Add(const std::string& key,
           const ClientState& client_state,
           UpdateCallback callback) override;
  void Update(const std::string& key,
              const ClientState& client_state,
              UpdateCallback callback) override;
  void Delete(const std::string& key, UpdateCallback callback) override;

  // Called when the proto database is initialized but no yet loading the data
  // into memory.
  void OnDbInitialized(LoadCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called after loading the data from database.
  void OnDataLoaded(LoadCallback callback,
                    bool success,
                    std::unique_ptr<EntryVector> entry_vector);

  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::ClientState, ClientState>>
      db_;
  base::WeakPtrFactory<ImpressionStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImpressionStore);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_STORE_H_
