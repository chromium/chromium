// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/proto/notification_entry.pb.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "components/leveldb_proto/public/proto_database.h"

// Forward declaration for proto conversion.
namespace leveldb_proto {
void DataToProto(notifications::NotificationEntry* entry,
                 notifications::proto::NotificationEntry* proto);

void ProtoToData(notifications::proto::NotificationEntry* proto,
                 notifications::NotificationEntry* entry);
}  // namespace leveldb_proto

namespace notifications {

class NotificationStore : public CollectionStore<NotificationEntry> {
 public:
  NotificationStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::NotificationEntry,
                                                   NotificationEntry>> db);
  NotificationStore(const NotificationStore&) = delete;
  NotificationStore& operator=(const NotificationStore&) = delete;
  ~NotificationStore() override;

 private:
  using KeyEntryVector = std::vector<std::pair<std::string, NotificationEntry>>;
  using KeyVector = std::vector<std::string>;
  using EntryVector = std::vector<NotificationEntry>;

  // CollectionStore<NotificationEntry> implementation.
  void InitAndLoad(LoadCallback callback) override;
  void Add(const std::string& key,
           const NotificationEntry& entry,
           UpdateCallback callback) override;
  void Update(const std::string& key,
              const NotificationEntry& entry,
              UpdateCallback callback) override;
  void Delete(const std::string& key, UpdateCallback callback) override;

  // Called when the proto database is initialized but no yet loading the data
  // into memory.
  void OnDbInitialized(LoadCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when data is loaded from |db_|.
  void OnDataLoaded(LoadCallback callback,
                    bool success,
                    std::unique_ptr<EntryVector> entry_vector);

  // Level db instance to persist data.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<proto::NotificationEntry, NotificationEntry>>
      db_;

  base::WeakPtrFactory<NotificationStore> weak_ptr_factory_{this};
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_STORE_H_
