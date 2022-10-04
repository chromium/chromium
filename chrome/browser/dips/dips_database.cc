// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_database.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

absl::optional<base::Time> ToOptionalTime(base::Time time) {
  if (time.is_null())
    return absl::nullopt;
  return time;
}

// Version number of the database.
// NOTE: When changing the version, add a new golden file for the new version
// and a test to verify that Init() works with it.
const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

}  // namespace

DIPSDatabase::DIPSDatabase(const absl::optional<base::FilePath>& db_path)
    : db_path_(db_path.value_or(base::FilePath())),
      db_(std::make_unique<sql::Database>(
          sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 32})) {
  base::AssertLongCPUWorkAllowed();
  if (db_path.has_value()) {
    DCHECK(!db_path->empty())
        << "To create an in-memory DIPSDatabase, explicitly pass an "
           "absl::nullopt file path.";
  }
}

DIPSDatabase::~DIPSDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Invoked on a db error.
void DIPSDatabase::DatabaseErrorCallback(int extended_error,
                                         sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO (jdh@): Try to recover corrupted databases, after we've added the
  // ability to store the database on disk.
  if (sql::IsErrorCatastrophic(extended_error)) {
    DCHECK_EQ(1, kCurrentVersionNumber);

    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndClose() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_->RazeAndClose();
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_->GetErrorMessage();
}

sql::InitStatus DIPSDatabase::OpenDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  db_->set_histogram_tag("DIPS");
  db_->set_error_callback(base::BindRepeating(
      &DIPSDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (in_memory()) {
    if (!db_->OpenInMemory())
      return sql::INIT_FAILURE;
  } else {
    if (!db_->Open(db_path_))
      return sql::INIT_FAILURE;
  }
  return sql::INIT_OK;
}

bool DIPSDatabase::InitTables() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kBounceSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS bounces("
          "site TEXT PRIMARY KEY NOT NULL,"
          "first_site_storage_time INTEGER NOT NULL,"
          "last_site_storage_time INTEGER NOT NULL,"
          "first_user_interaction_time INTEGER NOT NULL,"
          "last_user_interaction_time INTEGER NOT NULL)";
  // clang-format on

  DCHECK(db_->IsSQLValid(kBounceSql));
  return db_->Execute(kBounceSql);
}

sql::InitStatus DIPSDatabase::InitImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::InitStatus status = OpenDatabase();
  if (status != sql::INIT_OK) {
    return status;
  }

  DCHECK(db_->is_open());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Create the tables.
  sql::MetaTable meta_table;
  if (!meta_table.Init(db_.get(), kCurrentVersionNumber,
                       kCompatibleVersionNumber) ||
      !InitTables()) {
    db_->Close();
    return sql::INIT_FAILURE;
  }

  // Initialization is complete.
  if (!transaction.Commit())
    return sql::INIT_FAILURE;

  return sql::INIT_OK;
}

sql::InitStatus DIPSDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::InitStatus status = InitImpl();
  if (status == sql::INIT_OK) {
    return status;
  }

  db_->Close();

  return status;
}

bool DIPSDatabase::Write(const std::string& site,
                         absl::optional<base::Time> first_storage_time,
                         absl::optional<base::Time> last_storage_time,
                         absl::optional<base::Time> first_interaction_time,
                         absl::optional<base::Time> last_interaction_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kWriteSql[] =  // clang-format off
      "INSERT OR REPLACE INTO bounces("
          "site,"
          "first_site_storage_time,"
          "last_site_storage_time,"
          "first_user_interaction_time,"
          "last_user_interaction_time) "
          "VALUES (?,?,?,?,?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kWriteSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteSql));
  statement.BindString(0, site);
  statement.BindTime(1, first_storage_time.value_or(base::Time()));
  statement.BindTime(2, last_storage_time.value_or(base::Time()));
  statement.BindTime(3, first_interaction_time.value_or(base::Time()));
  statement.BindTime(4, last_interaction_time.value_or(base::Time()));

  return statement.Run();
}

absl::optional<StateValue> DIPSDatabase::Read(const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site,"
          "first_site_storage_time,"
          "last_site_storage_time,"
          "first_user_interaction_time,"
          "last_user_interaction_time "
          "FROM bounces WHERE site=?";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindString(0, site);

  if (!statement.Step()) {
    return absl::nullopt;
  }

  return StateValue{ToOptionalTime(statement.ColumnTime(1)),
                    ToOptionalTime(statement.ColumnTime(2)),
                    ToOptionalTime(statement.ColumnTime(3)),
                    ToOptionalTime(statement.ColumnTime(4))};
}

bool DIPSDatabase::RemoveRow(const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kRemoveSql[] = "DELETE FROM bounces WHERE site=?";
  DCHECK(db_->IsSQLValid(kRemoveSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kRemoveSql));
  statement.BindString(0, site);

  return statement.Run();
}
