// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_database.h"

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "sql/database.h"

namespace {

const char kLCPElementTableName[] = "lcp_element";

// When updating the database's schema, please increment the schema version.
constexpr int kCurrentSchemaVersion = 1;

std::unique_ptr<LCPCriticalPathPredictorDatabase>
InitializeDatabaseOnDbSequence(
    std::unique_ptr<LCPCriticalPathPredictorDatabase> database,
    base::OnceCallback<bool(sql::Database*)> db_opener) {
  database->InitializeMembersOnDbSequence(std::move(db_opener));
  return database;
}

}  // namespace

void LCPCriticalPathPredictorDatabase::Create(
    base::OnceCallback<bool(sql::Database*)> db_opener,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    base::TimeDelta flush_delay_for_writes,
    base::OnceCallback<void(std::unique_ptr<LCPCriticalPathPredictorDatabase>)>
        on_done_initializing) {
  CHECK(db_opener);
  CHECK(on_done_initializing);

  std::unique_ptr<LCPCriticalPathPredictorDatabase> database =
      std::make_unique<LCPCriticalPathPredictorDatabase>(
          db_task_runner, flush_delay_for_writes);

  // Because LCPCriticalPathPredictorDatabases are only constructed through an
  // asynchronous factory method, they are impossible to delete prior to their
  // initialization concluding.
  db_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitializeDatabaseOnDbSequence, std::move(database),
                     std::move(db_opener)),
      std::move(on_done_initializing));
}

LCPCriticalPathPredictorDatabase::~LCPCriticalPathPredictorDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_task_runner_->DeleteSoon(FROM_HERE, std::move(backing_database_));
}

sqlite_proto::KeyValueData<LCPElement>*
LCPCriticalPathPredictorDatabase::LCPElementData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lcp_element_data_.get();
}

LCPCriticalPathPredictorDatabase::LCPCriticalPathPredictorDatabase(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    base::TimeDelta flush_delay_for_writes)
    : table_manager_(base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
          db_task_runner)),
      db_task_runner_(std::move(db_task_runner)),
      backing_database_(std::make_unique<sql::Database>(sql::DatabaseOptions{
          // These options are boilerplate copied from
          // chrome/browser/predictor/predictor_database.cc.
          // TODO(crbug.com/1419756): Revisit this later.
          .exclusive_locking = true,
          .page_size = 4096,
          .cache_size = 500,
          .mmap_alt_status_discouraged = true,
          .enable_views_discouraged = true,  // Required by mmap_alt_status.
      })),
      lcp_element_table_(
          std::make_unique<sqlite_proto::KeyValueTable<LCPElement>>(
              kLCPElementTableName)),
      lcp_element_data_(
          std::make_unique<sqlite_proto::KeyValueData<LCPElement>>(
              table_manager_,
              lcp_element_table_.get(),
              // TODO(crbug.com/1419756): Set max_num_entries
              /*max_num_entries=*/absl::nullopt,
              flush_delay_for_writes)) {
  backing_database_->set_histogram_tag("LCPCriticalPathPredictor");
}

void LCPCriticalPathPredictorDatabase::InitializeMembersOnDbSequence(
    base::OnceCallback<bool(sql::Database*)> db_opener) {
  CHECK(db_task_runner_->RunsTasksInCurrentSequence());

  // Opens the backing database by passing it to `db_opener`, then calls into
  // ProtoTableManager and KeyValueData's on-database-sequence initialization
  // methods (the former in order to create the tables and execute a schema
  // upgrade if necessary, the latter in order to read data into memory).

  if (backing_database_ && !std::move(db_opener).Run(backing_database_.get())) {
    // Giving a nullptr database to ProtoTableManager results in the
    // operations it executes no-opping, so KeyValueData will fall back to
    // reasonable behavior of caching operations' results in memory but not
    // writing them to disk.
    backing_database_.reset();
  }

  CHECK(!backing_database_ || backing_database_->is_open());

  if (backing_database_) {
    backing_database_->Preload();
  }

  table_manager_->InitializeOnDbSequence(
      backing_database_.get(), std::vector<std::string>{kLCPElementTableName},
      kCurrentSchemaVersion);

  lcp_element_data_->InitializeOnDBSequence();
}
