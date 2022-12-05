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

// See comments at declaration of these variables in dips_database.h
// for details.

const base::TimeDelta DIPSDatabase::kMaxAge = base::Days(180);
const base::TimeDelta DIPSDatabase::kMetricsInterval = base::Hours(24);

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
           "absl::nullopt `db_path`.";
  }

  if (Init() != sql::INIT_OK)
    LOG(WARNING) << "Failed to initialize the DIPS SQLite database.";
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
          "last_user_interaction_time INTEGER NOT NULL,"
          "first_stateful_bounce_time INTEGER NOT NULL,"
          "last_stateful_bounce_time INTEGER NOT NULL,"
          "first_stateless_bounce_time INTEGER NOT NULL,"
          "last_stateless_bounce_time INTEGER NOT NULL)";
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

  last_health_metrics_time_ = base::Time::Now();
  ComputeDatabaseMetrics();

  return status;
}

void DIPSDatabase::ComputeDatabaseMetrics() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks start_time = base::TimeTicks::Now();

  int64_t db_size;
  if (base::GetFileSize(db_path_, &db_size)) {
    base::UmaHistogramMemoryKB("Privacy.DIPS.DatabaseSize", db_size / 1024);
  }

  base::UmaHistogramCounts10000("Privacy.DIPS.DatabaseEntryCount",
                                GetEntryCount());

  base::UmaHistogramTimes("Privacy.DIPS.DatabaseHealthMetricsTime",
                          base::TimeTicks::Now() - start_time);
}

bool DIPSDatabase::CheckDBInit() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open())
    return false;

  // Computing these metrics may be costly, so we only do it every
  // |kMetricsInterval|.
  base::Time now = base::Time::Now();
  if (now > last_health_metrics_time_ + kMetricsInterval) {
    last_health_metrics_time_ = now;
    ComputeDatabaseMetrics();
  }

  return true;
}

bool DIPSDatabase::Write(const std::string& site,
                         const TimestampRange& storage_times,
                         const TimestampRange& interaction_times,
                         const TimestampRange& stateful_bounce_times,
                         const TimestampRange& stateless_bounce_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

  static constexpr char kWriteSql[] =  // clang-format off
      "INSERT OR REPLACE INTO bounces("
          "site,"
          "first_site_storage_time,"
          "last_site_storage_time,"
          "first_user_interaction_time,"
          "last_user_interaction_time,"
          "first_stateful_bounce_time,"
          "last_stateful_bounce_time,"
          "first_stateless_bounce_time,"
          "last_stateless_bounce_time) "
          "VALUES (?,?,?,?,?,?,?,?,?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kWriteSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteSql));
  statement.BindString(0, site);
  statement.BindTime(1, storage_times.first.value_or(base::Time()));
  statement.BindTime(2, storage_times.last.value_or(base::Time()));
  statement.BindTime(3, interaction_times.first.value_or(base::Time()));
  statement.BindTime(4, interaction_times.last.value_or(base::Time()));
  statement.BindTime(5, stateful_bounce_times.first.value_or(base::Time()));
  statement.BindTime(6, stateful_bounce_times.last.value_or(base::Time()));
  statement.BindTime(7, stateless_bounce_times.first.value_or(base::Time()));
  statement.BindTime(8, stateless_bounce_times.last.value_or(base::Time()));
  return statement.Run();
}

absl::optional<StateValue> DIPSDatabase::Read(const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return absl::nullopt;

  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site,"
          "first_site_storage_time,"
          "last_site_storage_time,"
          "first_user_interaction_time,"
          "last_user_interaction_time,"
          "first_stateful_bounce_time,"
          "last_stateful_bounce_time,"
          "first_stateless_bounce_time,"
          "last_stateless_bounce_time "
          "FROM bounces WHERE site=?";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindString(0, site);

  if (!statement.Step()) {
    return absl::nullopt;
  }

  return StateValue{TimestampRange{ToOptionalTime(statement.ColumnTime(1)),
                                   ToOptionalTime(statement.ColumnTime(2))},
                    TimestampRange{ToOptionalTime(statement.ColumnTime(3)),
                                   ToOptionalTime(statement.ColumnTime(4))},
                    TimestampRange{ToOptionalTime(statement.ColumnTime(5)),
                                   ToOptionalTime(statement.ColumnTime(6))},
                    TimestampRange{ToOptionalTime(statement.ColumnTime(7)),
                                   ToOptionalTime(statement.ColumnTime(8))}};
}

std::vector<std::string> DIPSDatabase::GetAllSitesForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return {};

  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site FROM bounces ORDER BY site";
  // clang-format on

  DCHECK(db_->IsSQLValid(kReadSql));
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatBounced(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return {};

  DCHECK(last_interaction < range_start);
  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site FROM bounces "
        "WHERE (last_stateful_bounce_time > ? "
        "OR last_stateless_bounce_time > ?) AND "
        "last_user_interaction_time < ? AND "
        "last_user_interaction_time > 0 "
        "ORDER BY site";
  // clang-format on

  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindTime(0, range_start);
  statement.BindTime(1, range_start);
  statement.BindTime(2, last_interaction);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatBouncedWithState(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return {};

  DCHECK(last_interaction < range_start);
  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site FROM bounces "
        "WHERE last_stateful_bounce_time > ? AND "
        "last_site_storage_time > ? AND "
        "last_user_interaction_time < ? AND "
        "last_user_interaction_time > 0 "
        "ORDER BY site";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindTime(0, range_start);
  statement.BindTime(1, range_start);
  statement.BindTime(2, last_interaction);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

std::vector<std::string> DIPSDatabase::GetSitesThatUsedStorage(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return {};

  DCHECK(last_interaction < range_start);
  static constexpr char kReadSql[] =  // clang-format off
      "SELECT site FROM bounces "
        "WHERE (last_site_storage_time > ? OR "
        "last_stateful_bounce_time > ?) AND "
        "last_user_interaction_time < ? AND "
        "last_user_interaction_time > 0 "
        "ORDER BY site";
  // clang-format on
  DCHECK(db_->IsSQLValid(kReadSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kReadSql));
  statement.BindTime(0, range_start);
  statement.BindTime(1, range_start);
  statement.BindTime(2, last_interaction);

  std::vector<std::string> sites;
  while (statement.Step()) {
    sites.push_back(statement.ColumnString(0));
  }
  return sites;
}

bool DIPSDatabase::RemoveRow(const std::string& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

  static constexpr char kRemoveSql[] = "DELETE FROM bounces WHERE site=?";
  DCHECK(db_->IsSQLValid(kRemoveSql));

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kRemoveSql));
  statement.BindString(0, site);

  return statement.Run();
}

bool DIPSDatabase::RemoveEventsByTime(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  GarbageCollect();

  if (!ClearTimestamps(delete_begin, delete_end, type) ||
      !AdjustFirstTimestamps(delete_begin, delete_end, type) ||
      !AdjustLastTimestamps(delete_begin, delete_end, type))
    return false;

  transaction.Commit();
  return true;
}

bool DIPSDatabase::ClearTimestamps(const base::Time& delete_begin,
                                   const base::Time& delete_end,
                                   const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

  if (type == DIPSEventRemovalType::kAll) {
    static constexpr char kAllTypesSql[] =  // clang-format off
        "DELETE FROM bounces "
            "WHERE (first_site_storage_time>=?1 AND "
                   "last_site_storage_time<=?2 AND "
                   "first_user_interaction_time>=?1 AND "
                   "last_user_interaction_time<=?2 AND "
                   "first_stateful_bounce_time>=?1 AND "
                   "last_stateful_bounce_time<=?2 AND "
                   "first_stateless_bounce_time>=?1 AND "
                   "last_stateless_bounce_time<=?2) OR "

                   "(first_site_storage_time>=?1 AND "
                   "last_site_storage_time<=?2 AND "
                   "first_user_interaction_time=0 AND "
                   "last_user_interaction_time=0 AND "
                   "first_stateful_bounce_time=0 AND "
                   "last_stateful_bounce_time=0 AND "
                   "first_stateless_bounce_time=0 AND "
                   "last_stateless_bounce_time=0) OR "

                   "(first_site_storage_time=0 AND "
                   "last_site_storage_time=0 AND "
                   "first_user_interaction_time>=?1 AND "
                   "last_user_interaction_time<=?2 AND "
                   "first_stateful_bounce_time=0 AND "
                   "last_stateful_bounce_time=0 AND "
                   "first_stateless_bounce_time=0 AND "
                   "last_stateless_bounce_time=0) OR "

                   "(first_site_storage_time=0 AND "
                   "last_site_storage_time=0 AND "
                   "first_user_interaction_time=0 AND "
                   "last_user_interaction_time=0 AND "
                   "first_stateful_bounce_time>=?1 AND "
                   "last_stateful_bounce_time<=?2 AND "
                   "first_stateless_bounce_time=0 AND "
                   "last_stateless_bounce_time=0) OR "

                   "(first_site_storage_time=0 AND "
                   "last_site_storage_time=0 AND "
                   "first_user_interaction_time=0 AND "
                   "last_user_interaction_time=0 AND "
                   "first_stateful_bounce_time=0 AND "
                   "last_stateful_bounce_time=0 AND "
                   "first_stateless_bounce_time>=?1 AND "
                   "last_stateless_bounce_time<=?2)";
    // clang-format on
    DCHECK(db_->IsSQLValid(kAllTypesSql));

    sql::Statement s_all(db_->GetCachedStatement(SQL_FROM_HERE, kAllTypesSql));
    s_all.BindTime(0, delete_begin);
    s_all.BindTime(1, delete_end);

    if (!s_all.Run())
      return false;
  }

  if ((type & DIPSEventRemovalType::kHistory) ==
      DIPSEventRemovalType::kHistory) {
    static constexpr char kClearInteractionSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_user_interaction_time=0,"
            "last_user_interaction_time=0 "
            "WHERE first_user_interaction_time>=? AND "
                  "last_user_interaction_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearInteractionSql));

    sql::Statement s_clear_interaction(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearInteractionSql));
    s_clear_interaction.BindTime(0, delete_begin);
    s_clear_interaction.BindTime(1, delete_end);

    if (!s_clear_interaction.Run())
      return false;

    static constexpr char kClearStatelessSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_stateless_bounce_time=0,"
            "last_stateless_bounce_time=0 "
            "WHERE first_stateless_bounce_time>=? AND "
                  "last_stateless_bounce_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearStatelessSql));

    sql::Statement s_clear_stateless(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearStatelessSql));
    s_clear_stateless.BindTime(0, delete_begin);
    s_clear_stateless.BindTime(1, delete_end);

    if (!s_clear_stateless.Run())
      return false;
  }

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    static constexpr char kClearStorageSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_site_storage_time=0,"
            "last_site_storage_time=0 "
            "WHERE first_site_storage_time>=? AND "
                  "last_site_storage_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearStorageSql));

    sql::Statement s_clear_storage(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearStorageSql));
    s_clear_storage.BindTime(0, delete_begin);
    s_clear_storage.BindTime(1, delete_end);

    if (!s_clear_storage.Run())
      return false;

    static constexpr char kClearStatefulSql[] =  // clang-format off
        "UPDATE bounces SET "
            "first_stateful_bounce_time=0,"
            "last_stateful_bounce_time=0 "
            "WHERE first_stateful_bounce_time>=? AND "
                  "last_stateful_bounce_time<=?";
    // clang-format on
    DCHECK(db_->IsSQLValid(kClearStatefulSql));

    sql::Statement s_clear_stateful(
        db_->GetCachedStatement(SQL_FROM_HERE, kClearStatefulSql));
    s_clear_stateful.BindTime(0, delete_begin);
    s_clear_stateful.BindTime(1, delete_end);

    if (!s_clear_stateful.Run())
      return false;
  }

  static constexpr char kCleanUpSql[] =  // clang-format off
      "DELETE FROM bounces "
          "WHERE first_site_storage_time=0 AND "
                "last_site_storage_time=0 AND "
                "first_user_interaction_time=0 AND "
                "last_user_interaction_time=0 AND "
                "first_stateful_bounce_time=0 AND "
                "last_stateful_bounce_time=0 AND "
                "first_stateless_bounce_time=0 AND "
                "last_stateless_bounce_time=0";
  // clang-format on
  DCHECK(db_->IsSQLValid(kCleanUpSql));

  sql::Statement s_clean(db_->GetCachedStatement(SQL_FROM_HERE, kCleanUpSql));

  return s_clean.Run();
}

bool DIPSDatabase::AdjustFirstTimestamps(const base::Time& delete_begin,
                                         const base::Time& delete_end,
                                         const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

  if ((type & DIPSEventRemovalType::kHistory) ==
      DIPSEventRemovalType::kHistory) {
    static constexpr char kUpdateFirstInteractionSql[] =  // clang-format off
        "UPDATE bounces SET first_user_interaction_time=?1 "
            "WHERE first_user_interaction_time>=?2 AND "
                  "first_user_interaction_time<?1";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstInteractionSql));

    sql::Statement s_first_interaction(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstInteractionSql));
    s_first_interaction.BindTime(0, delete_end);
    s_first_interaction.BindTime(1, delete_begin);

    if (!s_first_interaction.Run())
      return false;

    static constexpr char kUpdateFirstStatelessSql[] =  // clang-format off
        "UPDATE bounces SET first_stateless_bounce_time=?1 "
            "WHERE first_stateless_bounce_time>=?2 AND "
                  "first_stateless_bounce_time<?1";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstStatelessSql));

    sql::Statement s_first_stateless(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstStatelessSql));
    s_first_stateless.BindTime(0, delete_end);
    s_first_stateless.BindTime(1, delete_begin);

    if (!s_first_stateless.Run())
      return false;
  }

  if ((type & DIPSEventRemovalType::kStorage) ==
      DIPSEventRemovalType::kStorage) {
    static constexpr char kUpdateFirstStorageSql[] =  // clang-format off
        "UPDATE bounces SET first_site_storage_time=?1 "
            "WHERE first_site_storage_time>=?2 AND "
                  "first_site_storage_time<?1";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstStorageSql));

    sql::Statement s_first_storage(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstStorageSql));
    s_first_storage.BindTime(0, delete_end);
    s_first_storage.BindTime(1, delete_begin);

    if (!s_first_storage.Run())
      return false;

    static constexpr char kUpdateFirstStatefulSql[] =  // clang-format off
        "UPDATE bounces SET first_stateful_bounce_time=?1 "
            "WHERE first_stateful_bounce_time>=?2 AND "
                  "first_stateful_bounce_time<?1";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateFirstStatefulSql));

    sql::Statement s_first_stateful(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFirstStatefulSql));
    s_first_stateful.BindTime(0, delete_end);
    s_first_stateful.BindTime(1, delete_begin);

    if (!s_first_stateful.Run())
      return false;
  }

  return true;
}

bool DIPSDatabase::AdjustLastTimestamps(const base::Time& delete_begin,
                                        const base::Time& delete_end,
                                        const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return false;

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

    if (!s_last_interaction.Run())
      return false;

    static constexpr char kUpdateLastStatelessSql[] =  // clang-format off
        "UPDATE bounces SET last_stateless_bounce_time=?1 "
            "WHERE last_stateless_bounce_time>?1 AND "
                  "last_stateless_bounce_time<=?2";
    // clang-format on
    DCHECK(db_->IsSQLValid(kUpdateLastStatelessSql));

    sql::Statement s_last_stateless(
        db_->GetCachedStatement(SQL_FROM_HERE, kUpdateLastStatelessSql));
    s_last_stateless.BindTime(0, delete_begin);
    s_last_stateless.BindTime(1, delete_end);

    if (!s_last_stateless.Run())
      return false;
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

    if (!s_last_storage.Run())
      return false;

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

    if (!s_last_stateful.Run())
      return false;
  }

  return true;
}

size_t DIPSDatabase::GetEntryCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return 0;

  sql::Statement s_entry_count(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM bounces"));
  return (s_entry_count.Step() ? s_entry_count.ColumnInt(0) : 0);
}

size_t DIPSDatabase::GarbageCollect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return 0;

  size_t num_entries = GetEntryCount();
  size_t num_deleted = 0;
  int purge_goal = num_entries - (max_entries_ - purge_entries_);

  if (num_entries <= max_entries_)
    return 0;

  DCHECK_GT(purge_goal, 0);
  num_deleted += GarbageCollectExpired();

  // If expiration did not purge enough entries, remove entries with the oldest
  // |last_user_interaction_time| until the |purge_goal| is satisfied.
  if (num_deleted < static_cast<size_t>(purge_goal)) {
    num_deleted += GarbageCollectOldest(purge_goal - num_deleted);
  }

  return num_deleted;
}

size_t DIPSDatabase::GarbageCollectExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return 0;

  base::Time safe_date(base::Time::Now() - kMaxAge);

  static constexpr char kExpireByInteractionSql[] =  // clang-format off
        "DELETE FROM bounces WHERE last_user_interaction_time<? AND "
                                  "last_user_interaction_time>0";
  // clang-format on
  DCHECK(db_->IsSQLValid(kExpireByInteractionSql));

  sql::Statement s_expire_by_interaction(
      db_->GetCachedStatement(SQL_FROM_HERE, kExpireByInteractionSql));
  s_expire_by_interaction.BindTime(0, safe_date);

  if (!s_expire_by_interaction.Run())
    return 0;

  return db_->GetLastChangeCount();
}

size_t DIPSDatabase::GarbageCollectOldest(int purge_goal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckDBInit())
    return 0;

  static constexpr char kGarbageCollectOldestSql[] =  // clang-format off
      "DELETE FROM bounces WHERE site "
          "IN(SELECT site FROM bounces "
              "ORDER BY "
                  "MAX(last_user_interaction_time,last_site_storage_time) ASC,"
                      "last_site_storage_time ASC "
              "LIMIT ?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kGarbageCollectOldestSql));

  sql::Statement s_garbage_collect_oldest(
      db_->GetCachedStatement(SQL_FROM_HERE, kGarbageCollectOldestSql));
  s_garbage_collect_oldest.BindInt(0, purge_goal);

  if (!s_garbage_collect_oldest.Run())
    return 0;

  return db_->GetLastChangeCount();
}
