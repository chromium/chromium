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
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
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

constexpr char kPrepopulatedKey[] = "prepopulated";

absl::optional<base::Time> ColumnOptionalTime(sql::Statement* statement,
                                              int column_index) {
  if (statement->GetColumnType(column_index) == sql::ColumnType::kNull) {
    return absl::nullopt;
  }
  return statement->ColumnTime(column_index);
}

TimestampRange RangeFromColumns(sql::Statement* statement,
                                int start_column_idx,
                                int end_column_idx,
                                std::vector<DIPSErrorCode>& errors) {
  absl::optional<base::Time> first_time =
      ColumnOptionalTime(statement, start_column_idx);
  absl::optional<base::Time> last_time =
      ColumnOptionalTime(statement, end_column_idx);

  if (!first_time.has_value() && !last_time.has_value()) {
    return absl::nullopt;
  }

  if (!first_time.has_value()) {
    errors.push_back(DIPSErrorCode::kRead_OpenEndedRange_NullStart);
    return absl::nullopt;
  }

  if (!last_time.has_value()) {
    errors.push_back(DIPSErrorCode::kRead_OpenEndedRange_NullEnd);
    return absl::nullopt;
  }

  return std::make_pair(first_time.value(), last_time.value());
}

// Binds either the start/ends of `range` or NULL at
// `start_param_idx`/`end_param_idx` in `statement` if time is provided.
void BindTimesOrNull(sql::Statement& statement,
                     TimestampRange time,
                     int start_param_idx,
                     int end_param_idx) {
  if (time.has_value()) {
    statement.BindTime(start_param_idx, time->first);
    statement.BindTime(end_param_idx, time->second);
  } else {
    statement.BindNull(start_param_idx);
    statement.BindNull(end_param_idx);
  }
}

// Version number of the database.
// NOTE: When changing the version, add a new golden file for the new version
// and a test to verify that Init() works with it.
const int kCurrentVersionNumber = 4;

// This number represents the min database version number with which this chrome
// code will be compatible with.
const int kCompatibleVersionNumber = 4;
}  // namespace

// See comments at declaration of these variables in dips_database.h
// for details.
const base::TimeDelta DIPSDatabase::kMetricsInterval = base::Hours(24);
const base::TimeDelta DIPSDatabase::kPopupTtl = base::Days(60);

DIPSDatabase::DIPSDatabase(const absl::optional<base::FilePath>& db_path)
    : db_path_(db_path.value_or(base::FilePath())),
      db_(std::make_unique<sql::Database>(
          sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 32})) {
  DCHECK(base::FeatureList::IsEnabled(dips::kFeature));
  base::AssertLongCPUWorkAllowed();
  if (db_path.has_value()) {
    DCHECK(!db_path->empty())
        << "To create an in-memory DIPSDatabase, explicitly pass an "
           "absl::nullopt `db_path`.";
  }

  if (Init() != sql::INIT_OK) {
    LOG(WARNING) << "Failed to initialize the DIPS SQLite database.";
  }
}

DIPSDatabase::~DIPSDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Invoked on a db error.
void DIPSDatabase::DatabaseErrorCallback(int extended_error,
                                         sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("Privacy.DIPS.DatabaseErrors", extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_->RazeAndPoison();
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << db_->GetErrorMessage();
  }
}

sql::InitStatus DIPSDatabase::OpenDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  // If this is not the first call to `OpenDatabase()` which can happen when
  // retrying the DB's initialization, then the error callback would've
  // previously been set.
  db_->reset_error_callback();

  db_->set_histogram_tag("DIPS");
  db_->set_error_callback(base::BindRepeating(
      &DIPSDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (in_memory()) {
    if (!db_->OpenInMemory()) {
      return sql::INIT_FAILURE;
    }
  } else {
    if (!db_->Open(db_path_)) {
      return sql::INIT_FAILURE;
    }
  }
  return sql::INIT_OK;
}

bool DIPSDatabase::InitTables() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kBouncesSql[] =  // clang-format off
    "CREATE TABLE bounces("
      "site TEXT PRIMARY KEY NOT NULL,"
      "first_site_storage_time INTEGER,"
      "last_site_storage_time INTEGER,"
      "first_user_interaction_time INTEGER,"
      "last_user_interaction_time INTEGER,"
      "first_stateful_bounce_time INTEGER,"
      "last_stateful_bounce_time INTEGER,"
      "first_bounce_time INTEGER,"
      "last_bounce_time INTEGER,"
      "first_web_authn_assertion_time INTEGER,"
      "last_web_authn_assertion_time INTEGER"
    ")";
  // clang-format on
  DCHECK(db_->IsSQLValid(kBouncesSql));

  static constexpr char kPopupsSql[] =  // clang-format off
    "CREATE TABLE popups("
      "opener_site TEXT NOT NULL,"
      "popup_site TEXT NOT NULL,"
      "access_id INT64,"
      "last_popup_time INTEGER,"
      "PRIMARY KEY (`opener_site`,`popup_site`)"
    ")";
  // clang-format on
  DCHECK(db_->IsSQLValid(kPopupsSql));

  return db_->Execute(kBouncesSql) && db_->Execute(kPopupsSql);
}

bool DIPSDatabase::MigrateToVersion2() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());
  // First make a new table that allows for null values in the timestamps
  // columns.
  static constexpr char kNewTableSql[] =  // clang-format off
      "CREATE TABLE new_bounces("
          "site TEXT PRIMARY KEY NOT NULL,"
          "first_site_storage_time INTEGER,"
          "last_site_storage_time INTEGER,"
          "first_user_interaction_time INTEGER,"
          "last_user_interaction_time INTEGER,"
          "first_stateful_bounce_time INTEGER,"
          "last_stateful_bounce_time INTEGER,"
          "first_stateless_bounce_time INTEGER,"
          "last_stateless_bounce_time INTEGER)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kNewTableSql));
  if (!db_->Execute(kNewTableSql)) {
    return false;
  }

  static constexpr char kCopyEverythingSql[] =
      "INSERT INTO new_bounces "
      "SELECT * FROM bounces";
  DCHECK(db_->IsSQLValid(kCopyEverythingSql));
  if (!db_->Execute(kCopyEverythingSql)) {
    return false;
  }

  const std::array<std::string, 8> timestamp_columns{
      "first_site_storage_time",     "last_site_storage_time",
      "first_user_interaction_time", "last_user_interaction_time",
      "first_stateless_bounce_time", "last_stateless_bounce_time",
      "first_stateful_bounce_time",  "last_stateful_bounce_time"};

  for (const std::string& column : timestamp_columns) {
    std::string command = base::StringPrintf(
        "UPDATE new_bounces "
        "SET %s=NULL "
        "WHERE %s=0 ",
        column.c_str(), column.c_str());
    sql::Statement s_nullify(db_->GetUniqueStatement(command.c_str()));

    if (!s_nullify.Run()) {
      return false;
    }
  }

  // Replace the first_stateless_bounce with the first bounce overall.
  // We have to first case on whether either of the bounce fields are NULL,
  // since MIN will return NULL if either are NULL.
  static constexpr char kReplaceFirstStatelessBounceSql[] =  // clang-format off
    "UPDATE new_bounces "
      "SET first_stateless_bounce_time = "
        "CASE "
          "WHEN first_stateful_bounce_time IS NULL "
            "THEN first_stateless_bounce_time "
          "WHEN first_stateless_bounce_time IS NULL "
            "THEN first_stateful_bounce_time "
          "ELSE MIN(first_stateful_bounce_time,first_stateless_bounce_time) "
        "END";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReplaceFirstStatelessBounceSql));
  if (!db_->Execute(kReplaceFirstStatelessBounceSql)) {
    return false;
  }

  // Replace the last_stateless_bounce with the last bounce overall.
  // We have to first case on whether either of the bounce fields are NULL,
  // since MAX will return NULL if either are NULL.
  static constexpr char kReplaceLastStatelessBounceSql[] =  // clang-format off
      "UPDATE new_bounces "
        "SET last_stateless_bounce_time = "
          "CASE "
            "WHEN last_stateful_bounce_time IS NULL "
              "THEN last_stateless_bounce_time "
            "WHEN last_stateless_bounce_time IS NULL "
              "THEN last_stateful_bounce_time "
            "ELSE MAX(last_stateful_bounce_time,last_stateless_bounce_time) "
          "END";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReplaceLastStatelessBounceSql));
  if (!db_->Execute(kReplaceLastStatelessBounceSql)) {
    return false;
  }
  // Rename this column to be reflect its new purpose.
  static constexpr char kRenameFirstStatelessBounceTimeSql[] =
      "ALTER TABLE new_bounces RENAME COLUMN first_stateless_bounce_time TO "
      "first_bounce_time";
  DCHECK(db_->IsSQLValid(kRenameFirstStatelessBounceTimeSql));
  if (!db_->Execute(kRenameFirstStatelessBounceTimeSql)) {
    return false;
  }

  // Rename this column to be reflect its new purpose.
  static constexpr char kRenameLastStatelessBounceTimeSql[] =
      "ALTER TABLE new_bounces RENAME COLUMN last_stateless_bounce_time TO "
      "last_bounce_time";
  if (!db_->Execute(kRenameLastStatelessBounceTimeSql)) {
    return false;
  }

  // Replace the old `bounces` table with the new one.
  static constexpr char kDropOldTableSql[] = "DROP TABLE bounces";
  if (!db_->Execute(kDropOldTableSql)) {
    return false;
  }

  static constexpr char kReplaceOldTable[] =
      "ALTER TABLE new_bounces RENAME TO bounces";
  if (!db_->Execute(kReplaceOldTable)) {
    return false;
  }

  return meta_table_.SetVersionNumber(2);
}

bool DIPSDatabase::MigrateToVersion3() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  return db_->Execute(
             "ALTER TABLE bounces ADD COLUMN first_web_authn_assertion_time "
             "INTEGER DEFAULT NULL") &&
         db_->Execute(
             "ALTER TABLE bounces ADD COLUMN last_web_authn_assertion_time "
             "INTEGER DEFAULT NULL") &&
         meta_table_.SetVersionNumber(3);
}

bool DIPSDatabase::MigrateToVersion4() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  static constexpr char kCreatePopupsTableSql[] =  // clang-format off
    "CREATE TABLE popups("
      "opener_site TEXT NOT NULL,"
      "popup_site TEXT NOT NULL,"
      "access_id INT64,"
      "last_popup_time INTEGER,"
      "PRIMARY KEY (`opener_site`,`popup_site`)"
    ")";
  // clang-format on
  DCHECK(db_->IsSQLValid(kCreatePopupsTableSql));

  return db_->Execute(kCreatePopupsTableSql) && meta_table_.SetVersionNumber(4);
}

bool DIPSDatabase::MigrateAsNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (int next_version = meta_table_.GetVersionNumber() + 1;
       next_version <= kCurrentVersionNumber; next_version++) {
    switch (next_version) {
      case 2:
        if (!MigrateToVersion2()) {
          return false;
        }
        if (!meta_table_.SetCompatibleVersionNumber(
                std::min(next_version, kCompatibleVersionNumber))) {
          return false;
        }
        break;
      case 3:
        if (!MigrateToVersion3()) {
          return false;
        }
        if (!meta_table_.SetCompatibleVersionNumber(
                std::min(next_version, kCompatibleVersionNumber))) {
          return false;
        }
        break;
      case 4:
        if (!MigrateToVersion4()) {
          return false;
        }
        if (!meta_table_.SetCompatibleVersionNumber(
                std::min(next_version, kCompatibleVersionNumber))) {
          return false;
        }
        break;
    }
  }
  return true;
}

sql::InitStatus DIPSDatabase::InitImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::InitStatus status = OpenDatabase();
  if (status != sql::INIT_OK) {
    return status;
  }
  DCHECK(db_->is_open());

  if (!sql::MetaTable::RazeIfIncompatible(
          db_.get(), sql::MetaTable::kNoLowestSupportedVersion,
          kCurrentVersionNumber)) {
    return sql::INIT_FAILURE;
  }

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return sql::INIT_FAILURE;
  }

  // Check if the table already exists to update schema if needed.
  bool table_already_exists = sql::MetaTable::DoesTableExist(db_.get());
  // Create the tables.
  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    db_->Close();
    return sql::INIT_FAILURE;
  }

  if (table_already_exists ? !MigrateAsNeeded() : !InitTables()) {
    return sql::INIT_FAILURE;
  }

  // Initialization is complete.
  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

sql::InitStatus DIPSDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::InitStatus status = InitImpl();
  int attempts = 1;

  if (status != sql::INIT_OK) {
    db_->Close();

    // Try to initialize the database once more in case it failed once and was
    // razed.
    status = InitImpl();
    attempts++;

    if (status != sql::INIT_OK) {
      attempts = 0;
    }
  }

  base::UmaHistogramExactLinear("Privacy.DIPS.DatabaseInit", attempts, 3);

  last_health_metrics_time_ = clock_->Now();
  LogDatabaseMetrics();

  return status;
}

void DIPSDatabase::LogDatabaseMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks start_time = base::TimeTicks::Now();

  int64_t db_size;
  if (base::GetFileSize(db_path_, &db_size)) {
    base::UmaHistogramMemoryKB("Privacy.DIPS.DatabaseSize", db_size / 1024);
  }

  base::UmaHistogramCounts10000("Privacy.DIPS.DatabaseEntryCount",
                                GetEntryCount(DIPSDatabaseTable::kBounces));

  base::UmaHistogramTimes("Privacy.DIPS.DatabaseHealthMetricsTime",
                          base::TimeTicks::Now() - start_time);
}

bool DIPSDatabase::CheckDBInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return false;
  }

  // Computing these metrics may be costly, so we only do it every
  // |kMetricsInterval|.
  base::Time now = clock_->Now();
  if (now > last_health_metrics_time_ + kMetricsInterval) {
    last_health_metrics_time_ = now;
    LogDatabaseMetrics();
  }

  return true;
}

bool DIPSDatabase::ExecuteSqlForTesting(const char* sql) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }
  return db_->ExecuteScriptForTesting(sql);  // IN-TEST
}

bool DIPSDatabase::Write(const std::string& site,
                         const TimestampRange& storage_times,
                         const TimestampRange& interaction_times,
                         const TimestampRange& stateful_bounce_times,
                         const TimestampRange& bounce_times,
                         const TimestampRange& web_authn_assertion_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      IsNullOrWithin(/*inner=*/stateful_bounce_times, /*outer=*/bounce_times));
  if (!CheckDBInit()) {
    return false;
  }

  if (site.empty()) {
    base::UmaHistogramEnumeration("Privacy.DIPS.DIPSErrorCodes",
                                  DIPSErrorCode::kWrite_EmptySite);
    return false;
  }

  static constexpr char kWriteSql[] =  // clang-format off
    "INSERT OR REPLACE INTO bounces("
      "site,"
      "first_site_storage_time,"
      "last_site_storage_time,"
      "first_user_interaction_time,"
      "last_user_interaction_time,"
      "first_stateful_bounce_time,"
      "last_stateful_bounce_time,"
      "first_bounce_time,"
      "last_bounce_time,"
      "first_web_authn_assertion_time,"
      "last_web_authn_assertion_time"
    ") VALUES(?,?,?,?,?,?,?,?,?,?,?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kWriteSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteSql));
  statement.BindString(0, site);
  BindTimesOrNull(statement, storage_times, 1, 2);
  BindTimesOrNull(statement, interaction_times, 3, 4);
  BindTimesOrNull(statement, stateful_bounce_times, 5, 6);
  BindTimesOrNull(statement, bounce_times, 7, 8);
  BindTimesOrNull(statement, web_authn_assertion_times, 9, 10);

  if (!statement.Run()) {
    return false;
  }

  base::UmaHistogramEnumeration("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kWrite_None);
  return true;
}

bool DIPSDatabase::WritePopup(const std::string& opener_site,
                              const std::string& popup_site,
                              const uint64_t access_id,
                              const base::Time& popup_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  static constexpr char kWriteSql[] =  // clang-format off
    "INSERT OR REPLACE INTO popups("
      "opener_site,"
      "popup_site,"
      "access_id,"
      "last_popup_time"
    ") VALUES(?,?,?,?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kWriteSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteSql));
  statement.BindString(0, opener_site);
  statement.BindString(1, popup_site);
  statement.BindInt64(2, access_id);
  statement.BindTime(3, popup_time);

  return statement.Run();
}

absl::optional<StateValue> DIPSDatabase::Read(const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return absl::nullopt;
  }

  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site,"
          "first_site_storage_time,"
          "last_site_storage_time,"
          "first_user_interaction_time,"
          "last_user_interaction_time,"
          "first_stateful_bounce_time,"
          "last_stateful_bounce_time,"
          "first_bounce_time,"
          "last_bounce_time,"
          "first_web_authn_assertion_time,"
          "last_web_authn_assertion_time "
          "FROM bounces WHERE site=?";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindString(0, site);

  if (!statement.Step()) {
    if (statement.Succeeded() && site.empty()) {
      base::UmaHistogramEnumeration("Privacy.DIPS.DIPSErrorCodes",
                                    DIPSErrorCode::kRead_EmptySite_NotInDb);
    }

    return absl::nullopt;
  }

  absl::optional<base::Time> last_user_interaction_time =
      ColumnOptionalTime(&statement, 4);
  absl::optional<base::Time> last_web_authn_assertion_time =
      ColumnOptionalTime(&statement, 10);
  // If the last interaction and last web authn assertion have expired, treat
  // this entry as not in the database so that callers rewrite the entry for
  // `site` as if it was deleted.
  if (HasExpired(last_user_interaction_time.has_value()
                     ? last_user_interaction_time
                     : last_web_authn_assertion_time) &&
      HasExpired(last_web_authn_assertion_time.has_value()
                     ? last_web_authn_assertion_time
                     : last_user_interaction_time)) {
    return absl::nullopt;
  }

  std::vector<DIPSErrorCode> errors;
  TimestampRange site_storage_times =
      RangeFromColumns(&statement, 1, 2, errors);
  TimestampRange user_interaction_times =
      RangeFromColumns(&statement, 3, 4, errors);
  TimestampRange stateful_bounce_times =
      RangeFromColumns(&statement, 5, 6, errors);
  TimestampRange bounce_times = RangeFromColumns(&statement, 7, 8, errors);
  TimestampRange web_authn_assertion_times =
      RangeFromColumns(&statement, 9, 10, errors);

  if (!IsNullOrWithin(stateful_bounce_times, bounce_times)) {
    DCHECK(stateful_bounce_times.has_value());
    errors.push_back(
        DIPSErrorCode::kRead_BounceTimesIsntSupersetOfStatefulBounces);
    if (!bounce_times.has_value()) {
      bounce_times = stateful_bounce_times;
    } else {
      base::Time start =
          std::min(stateful_bounce_times->first, bounce_times->first);
      base::Time end =
          std::max(stateful_bounce_times->second, bounce_times->second);
      bounce_times = {start, end};
    }
  }

  if (site.empty()) {
    errors.push_back(DIPSErrorCode::kRead_EmptySite_InDb);
  }

  if (errors.empty()) {
    base::UmaHistogramEnumeration("Privacy.DIPS.DIPSErrorCodes",
                                  DIPSErrorCode::kRead_None);
  } else {
    for (const DIPSErrorCode& error : errors) {
      base::UmaHistogramEnumeration("Privacy.DIPS.DIPSErrorCodes", error);
    }
  }

  // If `site` is an empty string, treat the entry as not in the database and
  // remove it. See crbug.com/1447035 for context.
  if (site.empty()) {
    RemoveRow(DIPSDatabaseTable::kBounces, site);
    return absl::nullopt;
  }

  return StateValue{site_storage_times, user_interaction_times,
                    stateful_bounce_times, bounce_times,
                    web_authn_assertion_times};
}

absl::optional<PopupsStateValue> DIPSDatabase::ReadPopup(
    const std::string& opener_site,
    const std::string& popup_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return absl::nullopt;
  }

  static constexpr char kReadSql[] =  // clang-format off
      "SELECT opener_site,"
        "popup_site,"
        "access_id,"
        "last_popup_time "
        "FROM popups "
        "WHERE opener_site=? AND popup_site=?";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindString(0, opener_site);
  statement.BindString(1, popup_site);

  if (!statement.Step()) {
    return absl::nullopt;
  }

  uint64_t access_id = statement.ColumnInt64(2);
  absl::optional<base::Time> popup_time = ColumnOptionalTime(&statement, 3);
  if (!popup_time.has_value()) {
    return absl::nullopt;
  }

  return PopupsStateValue{access_id, popup_time.value()};
}

std::vector<std::string> DIPSDatabase::GetAllSitesForTesting(
    DIPSDatabaseTable table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  std::vector<std::string> sites;

  if (table == DIPSDatabaseTable::kBounces) {
    static constexpr char kReadBounceTableSqlStr[] = "SELECT site FROM bounces";
    DCHECK(db_->IsSQLValid(kReadBounceTableSqlStr));
    sql::Statement s_bounces(
        db_->GetCachedStatement(SQL_FROM_HERE, kReadBounceTableSqlStr));
    while (s_bounces.Step()) {
      sites.push_back(s_bounces.ColumnString(0));
    }
  } else if (table == DIPSDatabaseTable::kPopups) {
    static constexpr char kReadPopupTableSqlStr[] =
        "SELECT opener_site,popup_site FROM popups";
    DCHECK(db_->IsSQLValid(kReadPopupTableSqlStr));
    sql::Statement s_popups(
        db_->GetCachedStatement(SQL_FROM_HERE, kReadPopupTableSqlStr));
    while (s_popups.Step()) {
      sites.push_back(s_popups.ColumnString(0));
      sites.push_back(s_popups.ColumnString(1));
    }
  }

  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatBounced(
    base::TimeDelta grace_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  ClearExpiredRows();

  static constexpr char kBounceSql[] =  // clang-format off
    "SELECT site FROM bounces "
    "WHERE "
      "first_bounce_time<? "
      "AND last_user_interaction_time IS NULL "
      "AND last_web_authn_assertion_time IS NULL "
    "ORDER BY site";
  // clang-format on
  DCHECK(db_->IsSQLValid(kBounceSql));
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kBounceSql));
  statement.BindTime(0, clock_->Now() - grace_period);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatBouncedWithState(
    base::TimeDelta grace_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  ClearExpiredRows();

  static constexpr char kStatefulBounceSql[] =  // clang-format off
    "SELECT site FROM bounces "
    "WHERE "
      "first_stateful_bounce_time<? "
      "AND last_user_interaction_time IS NULL "
      "AND last_web_authn_assertion_time IS NULL "
    "ORDER BY site";
  // clang-format on
  DCHECK(db_->IsSQLValid(kStatefulBounceSql));
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStatefulBounceSql));
  statement.BindTime(0, clock_->Now() - grace_period);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatUsedStorage(
    base::TimeDelta grace_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  ClearExpiredRows();

  static constexpr char kStorageSql[] =  // clang-format off
    "SELECT site FROM bounces "
    "WHERE "
      "first_site_storage_time<? "
      "AND last_user_interaction_time IS NULL "
      "AND last_web_authn_assertion_time IS NULL "
    "ORDER BY site";
  // clang-format on
  DCHECK(db_->IsSQLValid(kStorageSql));
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kStorageSql));
  statement.BindTime(0, clock_->Now() - grace_period);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::set<std::string> DIPSDatabase::FilterSitesWithInteractionOrWaa(
    const std::set<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  ClearExpiredRows();

  sql::Statement statement(db_->GetUniqueStatement(
      base::StrCat({"SELECT site,last_user_interaction_time,"
                    "last_web_authn_assertion_time FROM bounces "
                    "WHERE site IN(",
                    base::JoinString(
                        std::vector<base::StringPiece>(sites.size(), "?"), ","),
                    ")"})
          .c_str()));

  int i = 0;
  for (const auto& site : sites) {
    statement.BindString(i, site);
    i++;
  }

  std::set<std::string> interacted_sites;
  while (statement.Step()) {
    absl::optional<base::Time> last_user_interaction =
        ColumnOptionalTime(&statement, 1);
    absl::optional<base::Time> last_web_authn_assertion_time =
        ColumnOptionalTime(&statement, 2);

    if (last_user_interaction.has_value() ||
        last_web_authn_assertion_time.has_value()) {
      interacted_sites.insert(statement.ColumnString(0));
    }
  }
  return interacted_sites;
}

size_t DIPSDatabase::ClearExpiredRows() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clock_);
  if (!CheckDBInit()) {
    return false;
  }

  size_t change_count = 0;

  // NOTE: The SQLITE `MAX` and `MIN` return `NULL` if any value is `NULL`.
  // That's why `COALESCE` is used.
  static constexpr char kClearAllExpiredBouncesTableSql[] =  // clang-format off
    "DELETE FROM bounces "
    "WHERE MAX("
      "COALESCE(last_user_interaction_time,last_web_authn_assertion_time),"
      "COALESCE(last_web_authn_assertion_time,last_user_interaction_time)"
    ")<?";
  // clang-format on

  DCHECK(db_->IsSQLValid(kClearAllExpiredBouncesTableSql));
  sql::Statement bounces_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kClearAllExpiredBouncesTableSql));
  bounces_statement.BindTime(0, clock_->Now() - dips::kInteractionTtl.Get());
  if (!bounces_statement.Run()) {
    return 0;
  }
  change_count += db_->GetLastChangeCount();

  static constexpr char kClearAllExpiredPopupsTableSql[] =
      "DELETE FROM popups "
      "WHERE last_popup_time<?";

  DCHECK(db_->IsSQLValid(kClearAllExpiredPopupsTableSql));
  sql::Statement popups_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kClearAllExpiredPopupsTableSql));
  popups_statement.BindTime(0, clock_->Now() - kPopupTtl);
  if (!popups_statement.Run()) {
    return 0;
  }
  change_count += db_->GetLastChangeCount();

  return change_count;
}

bool DIPSDatabase::RemoveRow(const DIPSDatabaseTable table,
                             const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  ClearExpiredRows();

  if (table == DIPSDatabaseTable::kBounces) {
    static constexpr char kRemoveSql[] = "DELETE FROM bounces WHERE site=?";
    DCHECK(db_->IsSQLValid(kRemoveSql));
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kRemoveSql));
    statement.BindString(0, site);
    return statement.Run();
  } else if (table == DIPSDatabaseTable::kPopups) {
    static constexpr char kRemoveSql[] =
        "DELETE FROM popups WHERE opener_site=? OR popup_site=?";
    DCHECK(db_->IsSQLValid(kRemoveSql));
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kRemoveSql));
    statement.BindString(0, site);
    statement.BindString(1, site);
    return statement.Run();
  }

  // This should never be called - both DIPSDatabaseTable types are handled and
  // return above.
  return false;
}

bool DIPSDatabase::RemoveRows(const DIPSDatabaseTable table,
                              const std::vector<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  if (sites.empty()) {
    return true;
  }
  const std::string site_list =
      base::JoinString(std::vector<base::StringPiece>(sites.size(), "?"), ",");

  if (table == DIPSDatabaseTable::kBounces) {
    sql::Statement statement(db_->GetUniqueStatement(
        base::StrCat({"DELETE FROM bounces ", "WHERE site IN(", site_list, ")"})
            .c_str()));
    for (size_t i = 0; i < sites.size(); i++) {
      statement.BindString(i, sites[i]);
    }
    return statement.Run();
  } else if (table == DIPSDatabaseTable::kPopups) {
    sql::Statement statement(db_->GetUniqueStatement(
        base::StrCat({"DELETE FROM popups ", "WHERE opener_site IN(", site_list,
                      ") OR popup_site IN(", site_list, ")"})
            .c_str()));
    for (size_t i = 0; i < sites.size(); i++) {
      // There are 2 * sites.size() total bind locations, in the first and
      // second site_list. Each site should be bound in both lists.
      statement.BindString(i, sites[i]);
      statement.BindString(i + sites.size(), sites[i]);
    }
    return statement.Run();
  }

  // This should never be called - both DIPSDatabaseTable types are handled and
  // return above.
  return false;
}

bool DIPSDatabase::RemoveEventsByTime(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  ClearExpiredRows();

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  GarbageCollect();

  return (ClearTimestamps(delete_begin, delete_end, type) &&
          transaction.Commit());
}

bool DIPSDatabase::RemoveEventsBySite(bool preserve,
                                      const std::vector<std::string>& sites,
                                      const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  GarbageCollect();

  if (!ClearTimestampsBySite(preserve, sites, type)) {
    return false;
  }

  return transaction.Commit();
}

bool DIPSDatabase::ClearTimestamps(const base::Time& delete_begin,
                                   const base::Time& delete_end,
                                   const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  ClearExpiredRows();

  if ((type & DIPSEventRemovalType::kHistory) ==
      DIPSEventRemovalType::kHistory) {
    static constexpr char kClearInteractionSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_user_interaction_time=NULL,"
            "last_user_interaction_time=NULL "
            "WHERE first_user_interaction_time>=? AND "
                  "last_user_interaction_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearInteractionSql));

    sql::Statement s_clear_interaction(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearInteractionSql));
    s_clear_interaction.BindTime(0, delete_begin);
    s_clear_interaction.BindTime(1, delete_end);

    if (!s_clear_interaction.Run()) {
      return false;
    }

    static constexpr char kClearWaaSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_web_authn_assertion_time=NULL,"
            "last_web_authn_assertion_time=NULL "
            "WHERE first_web_authn_assertion_time>=? AND "
                  "last_web_authn_assertion_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearWaaSql));

    sql::Statement s_clear_waa(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearWaaSql));
    s_clear_waa.BindTime(0, delete_begin);
    s_clear_waa.BindTime(1, delete_end);

    if (!s_clear_waa.Run()) {
      return false;
    }

    static constexpr char kClearPopupsSql[] =  // clang-format off
        "DELETE FROM popups "
        "WHERE last_popup_time>=? AND last_popup_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearPopupsSql));

    sql::Statement s_clear_popups(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearPopupsSql));
    s_clear_popups.BindTime(0, delete_begin);
    s_clear_popups.BindTime(1, delete_end);

    if (!s_clear_popups.Run()) {
      return false;
    }
  }

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    static constexpr char kClearStorageSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_site_storage_time=NULL,"
            "last_site_storage_time=NULL "
            "WHERE first_site_storage_time>=? AND "
                  "last_site_storage_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearStorageSql));

    sql::Statement s_clear_storage(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearStorageSql));
    s_clear_storage.BindTime(0, delete_begin);
    s_clear_storage.BindTime(1, delete_end);

    if (!s_clear_storage.Run()) {
      return false;
    }

    static constexpr char kClearStatefulSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_stateful_bounce_time=NULL,"
            "last_stateful_bounce_time=NULL "
            "WHERE first_stateful_bounce_time>=? AND "
                  "last_stateful_bounce_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearStatefulSql));

    sql::Statement s_clear_stateful(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearStatefulSql));
    s_clear_stateful.BindTime(0, delete_begin);
    s_clear_stateful.BindTime(1, delete_end);

    if (!s_clear_stateful.Run()) {
      return false;
    }

    static constexpr char kClearBounceSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_bounce_time=NULL,"
            "last_bounce_time=NULL "
            "WHERE first_bounce_time>=? AND "
                  "last_bounce_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearBounceSql));

    sql::Statement s_clear_bounce(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearBounceSql));
    s_clear_bounce.BindTime(0, delete_begin);
    s_clear_bounce.BindTime(1, delete_end);

    if (!s_clear_bounce.Run()) {
      return false;
    }
  }

  return (RemoveEmptyRows() &&
          AdjustFirstTimestamps(delete_begin, delete_end, type) &&
          AdjustLastTimestamps(delete_begin, delete_end, type));
}

bool DIPSDatabase::AdjustFirstTimestamps(const base::Time& delete_begin,
                                         const base::Time& delete_end,
                                         const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  ClearExpiredRows();

  if (delete_end == base::Time::Max()) {
    // When `delete_end` is `base::Time::Max()`, any timestamp range that would
    // be altered by the below queries should have already been removed by
    // ClearTimestamps(), which MUST always be called before this method.
    return true;
  }

  if ((type & DIPSEventRemovalType::kHistory) ==
      DIPSEventRemovalType::kHistory) {
    static constexpr char kUpdateFirstInteractionSql[] =  // clang-format off
        "UPDATE bounces SET first_user_interaction_time=?2 "
            "WHERE first_user_interaction_time>=?1 AND "
                  "first_user_interaction_time<?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstInteractionSql));

    sql::Statement s_first_interaction(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstInteractionSql));
    s_first_interaction.BindTime(0, delete_begin);
    s_first_interaction.BindTime(1, delete_end);

    if (!s_first_interaction.Run()) {
      return false;
    }

    static constexpr char kUpdateFirstWaaSql[] =  // clang-format off
        "UPDATE bounces SET first_web_authn_assertion_time=?2 "
            "WHERE first_web_authn_assertion_time>=?1 AND "
                  "first_web_authn_assertion_time<?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstWaaSql));

    sql::Statement s_first_waa(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstWaaSql));
    s_first_waa.BindTime(0, delete_begin);
    s_first_waa.BindTime(1, delete_end);

    if (!s_first_waa.Run()) {
      return false;
    }
  }

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    static constexpr char kUpdateFirstStorageSql[] =  // clang-format off
        "UPDATE bounces SET first_site_storage_time=?2 "
            "WHERE first_site_storage_time>=?1 AND "
                  "first_site_storage_time<?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstStorageSql));

    sql::Statement s_first_storage(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstStorageSql));
    s_first_storage.BindTime(0, delete_begin);
    s_first_storage.BindTime(1, delete_end);

    if (!s_first_storage.Run()) {
      return false;
    }

    static constexpr char kUpdateFirstStatefulSql[] =  // clang-format off
        "UPDATE bounces SET first_stateful_bounce_time=?2 "
            "WHERE first_stateful_bounce_time>=?1 AND "
                  "first_stateful_bounce_time<?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstStatefulSql));

    sql::Statement s_first_stateful(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstStatefulSql));
    s_first_stateful.BindTime(0, delete_begin);
    s_first_stateful.BindTime(1, delete_end);

    if (!s_first_stateful.Run()) {
      return false;
    }

    static constexpr char kUpdateFirstBounceSql[] =  // clang-format off
        "UPDATE bounces SET first_bounce_time=?2 "
            "WHERE first_bounce_time>=?1 AND "
                  "first_bounce_time<?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstBounceSql));

    sql::Statement s_first_bounce(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstBounceSql));
    s_first_bounce.BindTime(0, delete_begin);
    s_first_bounce.BindTime(1, delete_end);

    if (!s_first_bounce.Run()) {
      return false;
    }
  }

  return true;
}

bool DIPSDatabase::AdjustLastTimestamps(const base::Time& delete_begin,
                                        const base::Time& delete_end,
                                        const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }

  ClearExpiredRows();

  if (delete_begin == base::Time::Min()) {
    // When `delete_begin` is `base::Time::Min()`, any timestamp range that
    // would be altered by the below queries should have already been removed by
    // ClearTimestamps(), which MUST always be called before this method.
    return true;
  }

  if ((type & DIPSEventRemovalType::kHistory) ==
      DIPSEventRemovalType::kHistory) {
    static constexpr char kUpdateLastInteractionSql[] =  // clang-format off
        "UPDATE bounces SET last_user_interaction_time=?1 "
            "WHERE last_user_interaction_time>?1 AND "
                  "last_user_interaction_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastInteractionSql));

    sql::Statement s_last_interaction(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastInteractionSql));
    s_last_interaction.BindTime(0, delete_begin);
    s_last_interaction.BindTime(1, delete_end);

    if (!s_last_interaction.Run()) {
      return false;
    }

    static constexpr char kUpdateLastWaaSql[] =  // clang-format off
        "UPDATE bounces SET last_web_authn_assertion_time=?1 "
            "WHERE last_web_authn_assertion_time>?1 AND "
                  "last_web_authn_assertion_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastWaaSql));

    sql::Statement s_last_waa(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastWaaSql));
    s_last_waa.BindTime(0, delete_begin);
    s_last_waa.BindTime(1, delete_end);

    if (!s_last_waa.Run()) {
      return false;
    }
  }

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    static constexpr char kUpdateLastStorageSql[] =  // clang-format off
        "UPDATE bounces SET last_site_storage_time=?1 "
            "WHERE last_site_storage_time>?1 AND "
                  "last_site_storage_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastStorageSql));

    sql::Statement s_last_storage(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastStorageSql));
    s_last_storage.BindTime(0, delete_begin);
    s_last_storage.BindTime(1, delete_end);

    if (!s_last_storage.Run()) {
      return false;
    }

    static constexpr char kUpdateLastStatefulSql[] =  // clang-format off
        "UPDATE bounces SET last_stateful_bounce_time=?1 "
            "WHERE last_stateful_bounce_time>?1 AND "
                  "last_stateful_bounce_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastStatefulSql));

    sql::Statement s_last_stateful(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastStatefulSql));
    s_last_stateful.BindTime(0, delete_begin);
    s_last_stateful.BindTime(1, delete_end);

    if (!s_last_stateful.Run()) {
      return false;
    }

    static constexpr char kUpdateLastBounceSql[] =  // clang-format off
        "UPDATE bounces SET last_bounce_time=?1 "
            "WHERE last_bounce_time>?1 AND "
                  "last_bounce_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastBounceSql));

    sql::Statement s_last_bounce(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastBounceSql));
    s_last_bounce.BindTime(0, delete_begin);
    s_last_bounce.BindTime(1, delete_end);

    if (!s_last_bounce.Run()) {
      return false;
    }
  }

  return true;
}

bool DIPSDatabase::ClearTimestampsBySite(bool preserve,
                                         const std::vector<std::string>& sites,
                                         const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sites.empty()) {
    return true;
  }

  std::string placeholders =
      base::JoinString(std::vector<base::StringPiece>(sites.size(), "?"), ",");

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    sql::Statement s_clear_storage(db_->GetUniqueStatement(  // clang-format off
        base::StrCat({"UPDATE bounces SET "
                          "first_site_storage_time=NULL,"
                          "last_site_storage_time=NULL,"
                          "first_stateful_bounce_time=NULL,"
                          "last_stateful_bounce_time=NULL,"
                          "first_bounce_time=NULL,"
                          "last_bounce_time=NULL "
                          "WHERE site ", (preserve ? "NOT " : ""),
                              "IN(", placeholders, ")" })  // clang-format on
            .c_str()));

    for (size_t i = 0; i < sites.size(); i++) {
      s_clear_storage.BindString(i, sites[i]);
    }

    if (!s_clear_storage.Run()) {
      return false;
    }
  }

  return RemoveEmptyRows();
}

bool DIPSDatabase::RemoveEmptyRows() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kCleanUpSql[] =  // clang-format off
    "DELETE FROM bounces "
    "WHERE first_site_storage_time IS NULL "
      "AND last_site_storage_time IS NULL "
      "AND first_user_interaction_time IS NULL "
      "AND last_user_interaction_time IS NULL "
      "AND first_stateful_bounce_time IS NULL "
      "AND last_stateful_bounce_time IS NULL "
      "AND first_bounce_time IS NULL "
      "AND last_bounce_time IS NULL "
      "AND first_web_authn_assertion_time IS NULL "
      "AND last_web_authn_assertion_time IS NULL";
  // clang-format on
  DCHECK(db_->IsSQLValid(kCleanUpSql));
  sql::Statement s_clean(db_->GetCachedStatement(SQL_FROM_HERE, kCleanUpSql));

  // Clearing the `popups` table is unnecessary because there are no operations
  // to nullify individual rows.

  return s_clean.Run();
}

size_t DIPSDatabase::GetEntryCount(const DIPSDatabaseTable table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return 0;
  }

  ClearExpiredRows();

  if (table == DIPSDatabaseTable::kBounces) {
    static constexpr char kBounceTableEntryCountSqlStr[] =
        "SELECT COUNT(*) FROM bounces";
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kBounceTableEntryCountSqlStr));
    return (statement.Step() ? statement.ColumnInt(0) : 0);
  } else if (table == DIPSDatabaseTable::kPopups) {
    static constexpr char kPopupTableEntryCountSqlStr[] =
        "SELECT COUNT(*) FROM popups";
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kPopupTableEntryCountSqlStr));
    return (statement.Step() ? statement.ColumnInt(0) : 0);
  }

  // This should never be called - both DIPSDatabaseTable types are handled and
  // return above.
  return false;
}

size_t DIPSDatabase::GarbageCollect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return 0;
  }

  size_t num_deleted = ClearExpiredRows();

  for (const DIPSDatabaseTable table :
       {DIPSDatabaseTable::kBounces, DIPSDatabaseTable::kPopups}) {
    // NOTE: `GetEntryCount()` might perform other row deletions whilst
    // re-calling `ClearExpiredRows()`, but possible precision lost in the final
    // num_delete isn't deemed crucial.
    const size_t num_entries = GetEntryCount(table);
    if (num_entries <= max_entries_) {
      continue;
    }

    const int purge_goal = num_entries - (max_entries_ - purge_entries_);
    DCHECK_GT(purge_goal, 0);
    num_deleted += GarbageCollectOldest(table, purge_goal);
  }

  return num_deleted;
}

size_t DIPSDatabase::GarbageCollectOldest(const DIPSDatabaseTable table,
                                          int purge_goal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return 0;
  }

  if (table == DIPSDatabaseTable::kBounces) {
    static constexpr char kGarbageCollectOldestSql[] =  // clang-format off
    "DELETE FROM bounces "
    "WHERE site IN("
      "SELECT site FROM bounces "
      "ORDER BY "
        "MAX("
          "COALESCE("
            "last_user_interaction_time,"
            "last_web_authn_assertion_time,"
            "last_site_storage_time"
          "),"
          "COALESCE("
            "last_web_authn_assertion_time,"
            "last_user_interaction_time,"
            "last_site_storage_time"
          "),"
          "COALESCE("
            "last_site_storage_time,"
            "last_user_interaction_time,"
            "last_web_authn_assertion_time"
          ")"
        ") ASC,"
        "last_user_interaction_time ASC,"
        "last_web_authn_assertion_time ASC,"
        "last_site_storage_time ASC "
      "LIMIT ?"
    ")";
    // clang-format on
    DCHECK(db_->IsSQLValid(kGarbageCollectOldestSql));

    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kGarbageCollectOldestSql));
    statement.BindInt(0, purge_goal);
    return statement.Run() ? db_->GetLastChangeCount() : 0;
  } else if (table == DIPSDatabaseTable::kPopups) {
    static constexpr char kGarbageCollectOldestSql[] =  // clang-format off
    "DELETE FROM popups "
    "WHERE (opener_site,popup_site) IN("
      "SELECT opener_site,popup_site "
      "FROM popups "
      "ORDER BY last_popup_time ASC "
      "LIMIT ?"
    ")";
    // clang-format on
    DCHECK(db_->IsSQLValid(kGarbageCollectOldestSql));

    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kGarbageCollectOldestSql));
    statement.BindInt(0, purge_goal);
    return statement.Run() ? db_->GetLastChangeCount() : 0;
  }

  // This should never be called - both DIPSDatabaseTable types are handled and
  // return above.
  return false;
}

std::vector<std::string> DIPSDatabase::GetGarbageCollectOldestSitesForTesting(
    DIPSDatabaseTable table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return {};
  }

  std::vector<std::string> sites;
  if (table == DIPSDatabaseTable::kBounces) {
    static constexpr char kReadSql[] =  // clang-format off
    "SELECT site FROM bounces "
    "ORDER BY "
      "MAX("
        "COALESCE("
          "last_user_interaction_time,"
          "last_web_authn_assertion_time,"
          "last_site_storage_time"
        "),"
        "COALESCE("
          "last_web_authn_assertion_time,"
          "last_user_interaction_time,"
          "last_site_storage_time"
        "),"
        "COALESCE("
          "last_site_storage_time,"
          "last_user_interaction_time,"
          "last_web_authn_assertion_time"
        ")"
      ") ASC,"
      "last_user_interaction_time ASC,"
      "last_web_authn_assertion_time ASC,"
      "last_site_storage_time ASC";
    // clang-format on
    DCHECK(db_->IsSQLValid(kReadSql));

    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
    while (statement.Step()) {
      sites.push_back(statement.ColumnString(0));
    }
  } else if (table == DIPSDatabaseTable::kPopups) {
    static constexpr char kReadSql[] =
        "SELECT opener_site,popup_site "
        "FROM popups "
        "ORDER BY last_popup_time ASC";
    DCHECK(db_->IsSQLValid(kReadSql));

    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
    while (statement.Step()) {
      sites.push_back(statement.ColumnString(0));
    }
  }

  return sites;
}

bool DIPSDatabase::MarkAsPrepopulated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }
  return meta_table_.SetValue(kPrepopulatedKey, 1);
}

bool DIPSDatabase::IsPrepopulated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit()) {
    return false;
  }
  int result;
  bool has_key = meta_table_.GetValue(kPrepopulatedKey, &result);
  if (!has_key) {
    meta_table_.SetValue(kPrepopulatedKey, 0);
    return false;
  }
  return result;
}
