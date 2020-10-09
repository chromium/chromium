// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_

#include <queue>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

class PersistedStateDBFactory;
class PersistedStateDBFactoryTest;
class PersistedStateDBTest;

// PersistedStateDB is leveldb backend store for NonCriticalPersistedTabData.
// NonCriticalPersistedTabData is an extension of TabState where data for
// new features which are not critical to the core functionality of the app
// are acquired and persisted across restarts. The intended key format is
// <NonCriticalPersistedTabData id>_<Tab id>

// NonCriticalPersistedTabData is stored in key/value pairs.
class PersistedStateDB : public KeyedService {
 public:
  using KeyAndValue = std::pair<std::string, std::vector<uint8_t>>;

  // Callback when content is acquired
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete). This will be invoked on a different SequenceRunner
  // to TabStateDB.
  using OperationCallback = base::OnceCallback<void(bool)>;

  // Entry in the database
  using ContentEntry = leveldb_proto::ProtoDatabase<
      persisted_state_db::PersistedStateContentProto>::KeyEntryVector;

  ~PersistedStateDB() override;

  // Loads the content data for the key and passes them to the callback
  void LoadContent(const std::string& key, LoadCallback callback);

  // Inserts a value for a given key and passes the result (success/failure) to
  // OperationCallback
  void InsertContent(const std::string& key,
                     const std::vector<uint8_t>& value,
                     OperationCallback callback);

  // Deletes content in the database, matching all keys which have a prefix
  // that matches the key
  void DeleteContent(const std::string& key, OperationCallback callback);

  // Delete all content in the database
  void DeleteAllContent(OperationCallback callback);

 private:
  friend class ::PersistedStateDBTest;
  friend class ::PersistedStateDBFactory;
  friend class ::PersistedStateDBFactoryTest;

  // Initializes the database
  PersistedStateDB(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& profile_directory);

  // Used for tests
  explicit PersistedStateDB(
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          persisted_state_db::PersistedStateContentProto>> storage_database,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Passes back database status following database initialization
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);

  // Callback when content is loaded
  void OnLoadContent(
      LoadCallback callback,
      bool success,
      std::unique_ptr<
          std::vector<persisted_state_db::PersistedStateContentProto>> content);

  // Callback when an operation (e.g. insert or delete) is called
  void OnOperationCommitted(OperationCallback callback, bool success);

  // Returns true if initialization status of database is not yet known
  bool InitStatusUnknown() const;

  // Returns true if the database failed to initialize
  bool FailedToInit() const;

  // Status of the database initialization.
  base::Optional<leveldb_proto::Enums::InitStatus> database_status_;

  // The database for storing content storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<
      persisted_state_db::PersistedStateContentProto>>
      storage_database_;

  // Store operations until the database is initialized at which point
  // deferred_operations_ is flushed and all operations are executed.
  std::vector<base::OnceClosure> deferred_operations_;

  base::WeakPtrFactory<PersistedStateDB> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PersistedStateDB);
};

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
