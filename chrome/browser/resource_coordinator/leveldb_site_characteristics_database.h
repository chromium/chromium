// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace resource_coordinator {

// Manages a LevelDB database used by a site characteristic data store.
// TODO(sebmarchand):
//   - Constraint the size of the database: Use a background task to trim the
//     database if it becomes too big and ensure that this fail nicely when the
//     disk is full.
//   - Batch the write operations to reduce the number of I/O events.
//
// All the DB operations are done asynchronously on a sequence allowed to do
// I/O operations.
class LevelDBSiteCharacteristicsDatabase
    : public LocalSiteCharacteristicsDatabase {
 public:
  explicit LevelDBSiteCharacteristicsDatabase(const base::FilePath& db_path);

  ~LevelDBSiteCharacteristicsDatabase() override;

  // LocalSiteCharacteristicDatabase:
  void ReadSiteCharacteristicsFromDB(
      const url::Origin& origin,
      LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
          callback) override;
  void WriteSiteCharacteristicsIntoDB(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) override;
  void RemoveSiteCharacteristicsFromDB(
      const std::vector<url::Origin>& site_origins) override;
  void ClearDatabase() override;
  void GetDatabaseSize(GetDatabaseSizeCallback callback) override;

  bool DatabaseIsInitializedForTesting();

  // Returns a raw pointer to the database for testing purposes. Note that as
  // the DB operations are made on a separate sequence it's recommended to call
  // TaskEnvironment::RunUntilIdle before calling this function to ensure
  // that the database has been fully initialized. The LevelDB implementation is
  // thread safe.
  leveldb::DB* GetDBForTesting();

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

  DISALLOW_COPY_AND_ASSIGN(LevelDBSiteCharacteristicsDatabase);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LEVELDB_SITE_CHARACTERISTICS_DATABASE_H_
