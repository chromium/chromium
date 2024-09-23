// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"

namespace app_list {
namespace {

constexpr int kUninitializedDbVersionNumber = 1;

// These values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class Status {
  kOk = 0,
  kFailedToCreatDirectory = 1,
  kFailedToOpenDb = 2,
  kFailedToInitializeMetaTable = 3,
  kFailedToSetVersionNumber = 4,
  kFailedToMigrateSchema = 5,

  kMaxValue = kFailedToMigrateSchema,
};

void LogStatusUma(const std::string& name, Status status) {
  base::UmaHistogramEnumeration(name, status);
}

}  // namespace

SqlDatabase::SqlDatabase(
    const base::FilePath& path_to_db,
    const std::string& histogram_tag,
    int current_version_number,
    base::RepeatingCallback<int(SqlDatabase* db)> create_table_schema,
    base::RepeatingCallback<int(SqlDatabase* db, int current_version_number)>
        migrate_table_schema)
    : create_table_schema_(std::move(create_table_schema)),
      migrate_table_schema_(std::move(migrate_table_schema)),
      path_to_db_(path_to_db),
      histogram_tag_(histogram_tag),
      db_(sql::Database(sql::DatabaseOptions())),
      meta_table_(sql::MetaTable()),
      current_version_number_(current_version_number) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_GT(current_version_number_, 1);
  DCHECK(!path_to_db_.empty());
}

SqlDatabase::~SqlDatabase() = default;

bool SqlDatabase::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_.is_open());

  const base::FilePath dir = path_to_db_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Could not create a directory to the new database.";
    LogStatusUma(histogram_tag_, Status::kFailedToCreatDirectory);
    return false;
  }

  db_.set_histogram_tag(histogram_tag_);

  if (!db_.Open(path_to_db_)) {
    LOG(ERROR) << "Unable to open " << path_to_db_;
    LogStatusUma(histogram_tag_, Status::kFailedToOpenDb);
    RazeDb();
    return false;
  }

  // Either initializes a new meta table or loads it from the db if exists.
  if (!meta_table_.Init(&db_, kUninitializedDbVersionNumber,
                        kUninitializedDbVersionNumber)) {
    LogStatusUma(histogram_tag_, Status::kFailedToInitializeMetaTable);
    RazeDb();
    return false;
  }

  if (meta_table_.GetVersionNumber() == current_version_number_) {
    LogStatusUma(histogram_tag_, Status::kOk);
    return true;
  }

  if (meta_table_.GetVersionNumber() == kUninitializedDbVersionNumber) {
    const int new_version_number = create_table_schema_.Run(this);
    DCHECK_GT(new_version_number, 1);

    if (!meta_table_.SetVersionNumber(new_version_number) ||
        !meta_table_.SetCompatibleVersionNumber(new_version_number)) {
      LogStatusUma(histogram_tag_, Status::kFailedToSetVersionNumber);
      RazeDb();
      return false;
    }
    LogStatusUma(histogram_tag_, Status::kOk);
    return true;
  }

  if (!MigrateDatabaseSchema()) {
    LOG(ERROR) << "Unable to migrate the schema";
    LogStatusUma(histogram_tag_, Status::kFailedToMigrateSchema);
    RazeDb();
    return false;
  }
  // base::Unretained is safe because `this` owns (and therefore outlives) the
  // sql::Database held by `db_`. That is, `db_` calls the error callback and
  // if `this` destroyed then `db_` is destroyed, as well.
  db_.set_error_callback(base::BindRepeating(&SqlDatabase::OnErrorCallback,
                                             base::Unretained(this)));
  LogStatusUma(histogram_tag_, Status::kOk);
  return true;
}

bool SqlDatabase::MigrateDatabaseSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // May happen if the code migrated from dev to stable channel.
  if (meta_table_.GetVersionNumber() > current_version_number_) {
    LOG(ERROR) << "Database is too new. Deleting the db.";
    Close();
    return sql::Database::Delete(path_to_db_) && Initialize();
  }

  if (migrate_table_schema_.Run(this, meta_table_.GetVersionNumber()) <
      meta_table_.GetVersionNumber()) {
    LOG(ERROR) << "Failed to migrate the schema. Deleting the db.";
    RazeDb();
    return false;
  }

  return true;
}

void SqlDatabase::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.reset_error_callback();
  meta_table_.Reset();
  db_.Close();
}

void SqlDatabase::OnErrorCallback(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "Error Code: " << sql::ToSqliteResultCode(error);
  if (stmt) {
    LOG(ERROR) << "Statement: " << stmt->GetSQLStatement();
  }
  if (sql::IsErrorCatastrophic(error)) {
    LOG(ERROR) << "The error is catastrophic. Razing db.";
    RazeDb();
  }
}

std::unique_ptr<sql::Statement> SqlDatabase::GetStatementForQuery(
    const sql::StatementID& sql_from_here,
    base::cstring_view query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_.is_open()) {
    return nullptr;
  }

  DVLOG(1) << "Making statement for query: " << query;
  DCHECK(db_.IsSQLValid(query));
  return std::make_unique<sql::Statement>(
      db_.GetCachedStatement(sql_from_here, query));
}

bool SqlDatabase::RazeDb() {
  DVLOG(1) << "Razing db.";
  meta_table_.Reset();
  if (db_.is_open()) {
    db_.Poison();
    // Sometimes it fails to do it due to locks or open handles.
    if (!sql::Database::Delete(path_to_db_)) {
      return false;
    }
  }
  return true;
}

base::FilePath SqlDatabase::GetPathToDb() const {
  return path_to_db_;
}

}  // namespace app_list
