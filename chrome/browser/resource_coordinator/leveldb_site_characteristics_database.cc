// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/leveldb_site_characteristics_database.h"

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace resource_coordinator {

namespace {

const char kInitStatusHistogramLabel[] =
    "ResourceCoordinator.LocalDB.DatabaseInit";
const char kInitStatusAfterRepairHistogramLabel[] =
    "ResourceCoordinator.LocalDB.DatabaseInitAfterRepair";
const char kInitStatusAfterDeleteHistogramLabel[] =
    "ResourceCoordinator.LocalDB.DatabaseInitAfterDelete";

enum class InitStatus {
  kInitStatusOk,
  kInitStatusCorruption,
  kInitStatusIOError,
  kInitStatusUnknownError,
  kInitStatusMax
};

// Report the database's initialization status metrics.
void ReportInitStatus(const char* histogram_name,
                      const leveldb::Status& status) {
  if (status.ok()) {
    base::UmaHistogramEnumeration(histogram_name, InitStatus::kInitStatusOk,
                                  InitStatus::kInitStatusMax);
  } else if (status.IsCorruption()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusCorruption,
                                  InitStatus::kInitStatusMax);
  } else if (status.IsIOError()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusIOError,
                                  InitStatus::kInitStatusMax);
  } else {
    base::UmaHistogramEnumeration(histogram_name,
                                  InitStatus::kInitStatusUnknownError,
                                  InitStatus::kInitStatusMax);
  }
}

// Attempt to repair the database stored in |db_path|.
bool RepairDatabase(const std::string& db_path) {
  leveldb_env::Options options;
  options.reuse_logs = false;
  options.max_open_files = 0;
  bool repair_succeeded = leveldb::RepairDB(db_path, options).ok();
  UMA_HISTOGRAM_BOOLEAN("ResourceCoordinator.LocalDB.DatabaseRepair",
                        repair_succeeded);
  return repair_succeeded;
}

bool ShouldAttemptDbRepair(const leveldb::Status& status) {
  // A corrupt database might be repaired (some data might be loss but it's
  // better than losing everything).
  if (status.IsCorruption())
    return true;
  // An I/O error might be caused by a missing manifest, it's sometime possible
  // to repair this (some data might be loss).
  if (status.IsIOError())
    return true;

  return false;
}

struct DatabaseSizeResult {
  base::Optional<int64_t> num_rows;
  base::Optional<int64_t> on_disk_size_kb;
};

}  // namespace

// Version history:
//
// - {no version}:
//     - Initial launch of the Database.
// - 1:
//     - Ignore the title/favicon events happening during the first fews seconds
//       after a tab being loaded.
//     - Ignore the audio events happening during the first fews seconds after a
//       tab being backgrounded.
// - 2:
//     - Ignore events that happen shortly after a tab is backgrounded. This is
//       because such events are likely a response to a recent user action
//       rather than an attempt from the tab to communicate in background.
//       See https://crbug.com/1865601.
//
// Transform logic:
//     - From any version to v1: The database is erased entirely.
//     - From any version to v2: The database is erased entirely.
const size_t LevelDBSiteCharacteristicsDatabase::kDbVersion = 2U;

const char LevelDBSiteCharacteristicsDatabase::kDbMetadataKey[] =
    "database_metadata";

// Helper class used to run all the blocking operations posted by
// LocalSiteCharacteristicDatabase on a ThreadPool sequence with the
// |MayBlock()| trait.
//
// Instances of this class should only be destructed once all the posted tasks
// have been run, in practice it means that they should ideally be stored in a
// std::unique_ptr<AsyncHelper, base::OnTaskRunnerDeleter>.
class LevelDBSiteCharacteristicsDatabase::AsyncHelper {
 public:
  explicit AsyncHelper(const base::FilePath& db_path) : db_path_(db_path) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    // Setting |sync| to false might cause some data loss if the system crashes
    // but it'll make the write operations faster (no data will be lost if only
    // the process crashes).
    write_options_.sync = false;
  }
  ~AsyncHelper() = default;

  // Open the database from |db_path_| after creating it if it didn't exist,
  // this reset the database if it's not at the expected version.
  void OpenOrCreateDatabase();

  // Implementations of the DB manipulation functions of
  // LevelDBSiteCharacteristicsDatabase that run on a blocking sequence.
  base::Optional<SiteDataProto> ReadSiteCharacteristicsFromDB(
      const url::Origin& origin);
  void WriteSiteCharacteristicsIntoDB(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto);
  void RemoveSiteCharacteristicsFromDB(
      const std::vector<url::Origin>& site_origin);
  void ClearDatabase();
  // Returns a struct with unset fields on failure.
  DatabaseSizeResult GetDatabaseSize();

  bool DBIsInitialized() { return db_ != nullptr; }

  leveldb::DB* GetDBForTesting() {
    DCHECK(DBIsInitialized());
    return db_.get();
  }

 private:
  enum class OpeningType {
    // A new database has been created.
    kNewDb,
    // An existing database has been used.
    kExistingDb,
  };

  // Implementation for the OpenOrCreateDatabase function.
  OpeningType OpenOrCreateDatabaseImpl();

  // The on disk location of the database.
  const base::FilePath db_path_;
  // The connection to the LevelDB database.
  std::unique_ptr<leveldb::DB> db_;
  // The options to be used for all database read operations.
  leveldb::ReadOptions read_options_;
  // The options to be used for all database write operations.
  leveldb::WriteOptions write_options_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(AsyncHelper);
};

void LevelDBSiteCharacteristicsDatabase::AsyncHelper::OpenOrCreateDatabase() {
  OpeningType opening_type = OpenOrCreateDatabaseImpl();
  if (!db_)
    return;
  std::string db_metadata;
  leveldb::Status s = db_->Get(
      read_options_, LevelDBSiteCharacteristicsDatabase::kDbMetadataKey,
      &db_metadata);
  bool is_expected_version = false;
  if (s.ok()) {
    // The metadata only contains the version of the database as a size_t value
    // for now.
    size_t version = std::numeric_limits<size_t>::max();
    CHECK(base::StringToSizeT(db_metadata, &version));
    if (version == LevelDBSiteCharacteristicsDatabase::kDbVersion)
      is_expected_version = true;
  }
  // TODO(sebmarchand): Add a migration engine rather than flushing the database
  // for every version change, https://crbug.com/866540.
  if ((opening_type == OpeningType::kExistingDb) && !is_expected_version) {
    DLOG(ERROR) << "Invalid DB version, recreating it.";
    ClearDatabase();
    // The database might fail to open.
    if (!db_)
      return;
    opening_type = OpeningType::kNewDb;
  }
  if (opening_type == OpeningType::kNewDb) {
    std::string metadata =
        base::NumberToString(LevelDBSiteCharacteristicsDatabase::kDbVersion);
    s = db_->Put(write_options_,
                  LevelDBSiteCharacteristicsDatabase::kDbMetadataKey,
                  metadata);
    if (!s.ok()) {
      DLOG(ERROR) << "Error while inserting the metadata in the site "
                  << "characteristics database: " << s.ToString();
    }
  }
}

base::Optional<SiteDataProto>
LevelDBSiteCharacteristicsDatabase::AsyncHelper::ReadSiteCharacteristicsFromDB(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return base::nullopt;

  leveldb::Status s;
  std::string protobuf_value;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    s = db_->Get(read_options_, SerializeOriginIntoDatabaseKey(origin),
                 &protobuf_value);
  }
  base::Optional<SiteDataProto> site_characteristic_proto;
  if (s.ok()) {
    site_characteristic_proto = SiteDataProto();
    if (!site_characteristic_proto->ParseFromString(protobuf_value)) {
      site_characteristic_proto = base::nullopt;
      DLOG(ERROR) << "Error while trying to parse a SiteDataProto "
                  << "protobuf.";
    }
  }
  return site_characteristic_proto;
}

void LevelDBSiteCharacteristicsDatabase::AsyncHelper::
    WriteSiteCharacteristicsIntoDB(
        const url::Origin& origin,
        const SiteDataProto& site_characteristic_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return;

  leveldb::Status s;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    s = db_->Put(write_options_, SerializeOriginIntoDatabaseKey(origin),
                 site_characteristic_proto.SerializeAsString());
  }

  if (!s.ok()) {
    DLOG(ERROR)
        << "Error while inserting an element in the site characteristics "
        << "database: " << s.ToString();
  }
}

void LevelDBSiteCharacteristicsDatabase::AsyncHelper::
    RemoveSiteCharacteristicsFromDB(
        const std::vector<url::Origin>& site_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  leveldb::WriteBatch batch;
  for (const auto& iter : site_origins)
    batch.Delete(SerializeOriginIntoDatabaseKey(iter));
  leveldb::Status status = db_->Write(write_options_, &batch);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to remove some entries from the site "
                 << "characteristics database: " << status.ToString();
  }
}

void LevelDBSiteCharacteristicsDatabase::AsyncHelper::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  db_.reset();
  leveldb_env::Options options;
  leveldb::Status status = leveldb::DestroyDB(db_path_.AsUTF8Unsafe(), options);
  if (status.ok()) {
    OpenOrCreateDatabaseImpl();
  } else {
    LOG(WARNING) << "Failed to destroy the site characteristics database: "
                 << status.ToString();
  }
}

DatabaseSizeResult
LevelDBSiteCharacteristicsDatabase::AsyncHelper::GetDatabaseSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DatabaseSizeResult();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DatabaseSizeResult ret;
#if defined(OS_WIN)
  // Windows has an annoying mis-feature that the size of an open file is not
  // written to the parent directory until the file is closed. Since this is a
  // diagnostic interface that should be rarely called, go to the trouble of
  // closing and re-opening the database in order to get an up-to date size to
  // report.
  db_.reset();
#endif
  ret.on_disk_size_kb = base::ComputeDirectorySize(db_path_) / 1024;
#if defined(OS_WIN)
  OpenOrCreateDatabase();
  if (!db_)
    return DatabaseSizeResult();
#endif

  // Default read options will fill the cache as we go.
  std::unique_ptr<leveldb::Iterator> iterator(
      db_->NewIterator(leveldb::ReadOptions()));
  int64_t num_rows = 0;
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next())
    ++num_rows;

  ret.num_rows = num_rows;
  return ret;
}

LevelDBSiteCharacteristicsDatabase::AsyncHelper::OpeningType
LevelDBSiteCharacteristicsDatabase::AsyncHelper::OpenOrCreateDatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_) << "Database already open";
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  OpeningType opening_type = OpeningType::kNewDb;

  // Report the on disk size of the database if it already exists.
  if (base::DirectoryExists(db_path_)) {
    opening_type = OpeningType::kExistingDb;
    int64_t db_ondisk_size_in_bytes = base::ComputeDirectorySize(db_path_);
    UMA_HISTOGRAM_MEMORY_KB("ResourceCoordinator.LocalDB.OnDiskSize",
                            db_ondisk_size_in_bytes / 1024);
  }

  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::Status status =
      leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);

  ReportInitStatus(kInitStatusHistogramLabel, status);

  if (status.ok())
    return opening_type;

  if (!ShouldAttemptDbRepair(status))
    return opening_type;

  if (RepairDatabase(db_path_.AsUTF8Unsafe())) {
    status = leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);
    ReportInitStatus(kInitStatusAfterRepairHistogramLabel, status);
    if (status.ok())
      return opening_type;
  }

  // Delete the database and try to open it one last time.
  if (leveldb_chrome::DeleteDB(db_path_, options).ok()) {
    status = leveldb_env::OpenDB(options, db_path_.AsUTF8Unsafe(), &db_);
    ReportInitStatus(kInitStatusAfterDeleteHistogramLabel, status);
    if (!status.ok())
      db_.reset();
  }

  return opening_type;
}

LevelDBSiteCharacteristicsDatabase::LevelDBSiteCharacteristicsDatabase(
    const base::FilePath& db_path)
    : blocking_task_runner_(base::CreateSequencedTaskRunner(
          // The |BLOCK_SHUTDOWN| trait is required to ensure that a clearing of
          // the database won't be skipped.
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      async_helper_(new AsyncHelper(db_path),
                    base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LevelDBSiteCharacteristicsDatabase::
                                    AsyncHelper::OpenOrCreateDatabase,
                                base::Unretained(async_helper_.get())));
}

LevelDBSiteCharacteristicsDatabase::~LevelDBSiteCharacteristicsDatabase() =
    default;

void LevelDBSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDB(
    const url::Origin& origin,
    LocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDBCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Trigger the asynchronous task and make it run the callback on this thread
  // once it returns.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LevelDBSiteCharacteristicsDatabase::AsyncHelper::
                         ReadSiteCharacteristicsFromDB,
                     base::Unretained(async_helper_.get()), origin),
      base::BindOnce(std::move(callback)));
}

void LevelDBSiteCharacteristicsDatabase::WriteSiteCharacteristicsIntoDB(
    const url::Origin& origin,
    const SiteDataProto& site_characteristic_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LevelDBSiteCharacteristicsDatabase::
                                    AsyncHelper::WriteSiteCharacteristicsIntoDB,
                                base::Unretained(async_helper_.get()), origin,
                                std::move(site_characteristic_proto)));
}

void LevelDBSiteCharacteristicsDatabase::RemoveSiteCharacteristicsFromDB(
    const std::vector<url::Origin>& site_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBSiteCharacteristicsDatabase::AsyncHelper::
                         RemoveSiteCharacteristicsFromDB,
                     base::Unretained(async_helper_.get()),
                     std::move(site_origins)));
}

void LevelDBSiteCharacteristicsDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LevelDBSiteCharacteristicsDatabase::AsyncHelper::ClearDatabase,
          base::Unretained(async_helper_.get())));
}

void LevelDBSiteCharacteristicsDatabase::GetDatabaseSize(
    GetDatabaseSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Adapt the callback with a lambda to allow using PostTaskAndReplyWithResult.
  auto reply_callback = base::BindOnce(
      [](GetDatabaseSizeCallback callback, const DatabaseSizeResult& result) {
        std::move(callback).Run(result.num_rows, result.on_disk_size_kb);
      },
      std::move(callback));

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &LevelDBSiteCharacteristicsDatabase::AsyncHelper::GetDatabaseSize,
          base::Unretained(async_helper_.get())),
      std::move(reply_callback));
}

bool LevelDBSiteCharacteristicsDatabase::DatabaseIsInitializedForTesting() {
  return async_helper_->DBIsInitialized();
}

leveldb::DB* LevelDBSiteCharacteristicsDatabase::GetDBForTesting() {
  return async_helper_->GetDBForTesting();
}

}  // namespace resource_coordinator
