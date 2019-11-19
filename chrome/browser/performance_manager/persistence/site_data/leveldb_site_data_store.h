// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_manager {

// Manages a LevelDB database used by a site data store.
// TODO(sebmarchand):
//   - Constrain the size of the database: Use a background task to trim the
//     database if it becomes too big and ensure that this fails nicely when the
//     disk is full.
//   - Batch the write operations to reduce the number of I/O events.
//
// All the DB operations are done asynchronously on a sequence allowed to do
// I/O operations.
class LevelDBSiteDataStore : public SiteDataStore {
 public:
  explicit LevelDBSiteDataStore(const base::FilePath& db_path);

  ~LevelDBSiteDataStore() override;

  // SiteDataStore:
  void ReadSiteDataFromStore(
      const url::Origin& origin,
      SiteDataStore::ReadSiteDataFromStoreCallback callback) override;
  void WriteSiteDataIntoStore(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) override;
  void RemoveSiteDataFromStore(
      const std::vector<url::Origin>& site_origins) override;
  void ClearStore() override;
  void GetStoreSize(GetStoreSizeCallback callback) override;
  void SetInitializationCallbackForTesting(base::OnceClosure callback) override;

  bool DatabaseIsInitializedForTesting();

  // Returns a raw pointer to the database for testing purposes. Note that as
  // the DB operations are made on a separate sequence it's recommended to call
  // TaskEnvironment::RunUntilIdle before calling this function to ensure
  // that the database has been fully initialized. The LevelDB implementation is
  // thread safe.
  leveldb::DB* GetDBForTesting();

  // Make the new instances of this class use an in memory database rather than
  // creating it on disk.
  static std::unique_ptr<base::AutoReset<bool>> UseInMemoryDBForTesting();

  static const size_t kDbVersion;
  static const char kDbMetadataKey[];

 private:
  class AsyncHelper;

  // The task runner used to run all the blocking operations.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Helper object that should be used to trigger all the operations that need
  // to run on |blocking_task_runner_|, it is guaranteed that the AsyncHelper
  // held by this object will only be destructed once all the tasks that have
  // been posted to it have completed.
  std::unique_ptr<AsyncHelper, base::OnTaskRunnerDeleter> async_helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LevelDBSiteDataStore);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_
