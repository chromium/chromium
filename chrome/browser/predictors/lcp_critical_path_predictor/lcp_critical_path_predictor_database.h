// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_DATABASE_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_DATABASE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor.pb.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"

namespace sql {

class Database;

}  // namespace sql

// LCPCriticalPathPredictorDatabase does two things:
// 1. It constructs and initializes an SQLite database, delegating some of this
//    work to the //components/sqlite_proto library.
// 2. It provides, via the sqlite_proto::KeyValueData interface, access to a
//    table in the database that it owns.
//
// If the backing database fails to initialize, we still consider the
// LCPCriticalPathPredictorDatabase as having initialized "successfully", since
// it is safe to execute operations in this single-session fallback state. In
// this case, this class works as in-memory database, not writing data to disk.
class LCPCriticalPathPredictorDatabase final {
 public:
  // Constructs and asynchronously initializes a new
  // LCPCriticalPathPredictorDatabase, calling `on_done_initializing` with an
  // owning pointer to the constructed object once initialization has finished
  // and the object is ready to use.
  //
  // Posts a task to `db_task_runner` to initialize all pertinent DB state on
  // the DB sequence.
  //
  // `db_opener` is a callback that opens the given sql::Database*.  This allows
  // opening in memory for testing (for instance). In normal usage, this will
  // probably open the given database on disk at a prespecified filepath.
  //
  // `flush_delay_for_writes` is the maximum time before each write is flushed
  // to the underlying database.
  static void Create(base::OnceCallback<bool(sql::Database*)> db_opener,
                     scoped_refptr<base::SequencedTaskRunner> db_task_runner,
                     base::TimeDelta flush_delay_for_writes,
                     base::OnceCallback<void(
                         std::unique_ptr<LCPCriticalPathPredictorDatabase>)>
                         on_done_initializing);

  // This constructor should be called only from `Create` static function.
  LCPCriticalPathPredictorDatabase(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      base::TimeDelta flush_delay_for_writes);

  ~LCPCriticalPathPredictorDatabase();

  LCPCriticalPathPredictorDatabase(const LCPCriticalPathPredictorDatabase&) =
      delete;
  LCPCriticalPathPredictorDatabase& operator=(
      const LCPCriticalPathPredictorDatabase&) = delete;

  // This should be called only from `Create` static function.
  void InitializeMembersOnDbSequence(
      base::OnceCallback<bool(sql::Database*)> db_opener);

  // Use this getter to execute operations (get, put, delete) on the
  // underlying data.
  sqlite_proto::KeyValueData<LCPElement>* LCPElementData();

 private:
  // `table_manager_` is responsible for constructing the database's tables and
  // scheduling database tasks.
  scoped_refptr<sqlite_proto::ProtoTableManager> table_manager_;

  // Keep a handle on the DB task runner so that the destructor
  // can use the DB sequence to clean up the DB.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // The backing database. This can be nullptr if the backing database fails to
  // initialize.
  std::unique_ptr<sql::Database> backing_database_;

  // KeyValueData/KeyValueTable pair is responsible for executing SQL
  // operations against a particular database table. The KeyValueTables help
  // with serializing/deserializing proto objects, and the KeyValueData objects
  // batch writes and cache reads.
  std::unique_ptr<sqlite_proto::KeyValueTable<LCPElement>> lcp_element_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<LCPElement>> lcp_element_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_DATABASE_H_
