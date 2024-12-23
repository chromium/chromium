// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_database_migrator.h"

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/dips/dips_database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

using internal::DIPSDatabaseMigrator;

DIPSDatabaseMigrator::DIPSDatabaseMigrator(sql::Database* const db,
                                           sql::MetaTable* const meta_table)
    : db_(CHECK_DEREF(db)), meta_table_(CHECK_DEREF(meta_table)) {}

bool MigrateDIPSSchemaToLatestVersion(sql::Database& db,
                                      sql::MetaTable& meta_table) {
  DIPSDatabaseMigrator migrator(&db, &meta_table);

  for (int next_version = meta_table.GetVersionNumber() + 1;
       next_version <= DIPSDatabase::kLatestSchemaVersion; next_version++) {
    switch (next_version) {
      case 2:
        if (!migrator.MigrateSchemaVersionFrom1To2()) {
          return false;
        }
        break;
      case 3:
        if (!migrator.MigrateSchemaVersionFrom2To3()) {
          return false;
        }
        break;
      case 4:
        if (!migrator.MigrateSchemaVersionFrom3To4()) {
          return false;
        }
        break;
      case 5:
        if (!migrator.MigrateSchemaVersionFrom4To5()) {
          return false;
        }
        break;
      case 6:
        if (!migrator.MigrateSchemaVersionFrom5To6()) {
          return false;
        }
        break;
      case 7:
        if (!migrator.MigrateSchemaVersionFrom6To7()) {
          return false;
        }
        break;
      case 8:
        if (!migrator.MigrateSchemaVersionFrom7To8()) {
          return false;
        }
        break;
    }
  }
  return true;
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom1To2() {
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
    sql::Statement s_nullify(db_->GetUniqueStatement(command));

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

  return meta_table_->SetVersionNumber(2) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(2, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom2To3() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  return db_->Execute(
             "ALTER TABLE bounces ADD COLUMN first_web_authn_assertion_time "
             "INTEGER DEFAULT NULL") &&
         db_->Execute(
             "ALTER TABLE bounces ADD COLUMN last_web_authn_assertion_time "
             "INTEGER DEFAULT NULL") &&
         meta_table_->SetVersionNumber(3) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(3, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom3To4() {
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

  return db_->Execute(kCreatePopupsTableSql) &&
         meta_table_->SetVersionNumber(4) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(4, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom4To5() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  return db_->Execute(
             "ALTER TABLE popups ADD COLUMN is_current_interaction "
             "BOOLEAN DEFAULT NULL") &&
         meta_table_->SetVersionNumber(5) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(5, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom5To6() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  static constexpr char kCreateConfigTableSql[] =  // clang-format off
    "CREATE TABLE config("
      "key TEXT NOT NULL,"
      "int_value INTEGER,"
      "PRIMARY KEY (`key`)"
    ")";
  // clang-format on
  DCHECK(db_->IsSQLValid(kCreateConfigTableSql));

  if (!db_->Execute(kCreateConfigTableSql)) {
    return false;
  }

  if (int result;
      meta_table_->GetValue(DIPSDatabase::kPrepopulatedKey, &result)) {
    static constexpr char kInsertValueSql[] =
        "INSERT OR REPLACE INTO config(key,int_value) VALUES(?,?)";
    DCHECK(db_->IsSQLValid(kInsertValueSql));
    sql::Statement statement(db_->GetUniqueStatement(kInsertValueSql));
    statement.BindString(0, DIPSDatabase::kPrepopulatedKey);
    statement.BindInt64(1, result);

    if (!statement.Run()) {
      return false;
    }

    if (!meta_table_->DeleteKey(DIPSDatabase::kPrepopulatedKey)) {
      return false;
    }
  }

  return meta_table_->SetVersionNumber(6) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(6, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom6To7() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_->HasActiveTransactions());

  static constexpr char kDeleteConfigSql[] = "DELETE FROM config WHERE key = ?";
  CHECK(db_->IsSQLValid(kDeleteConfigSql));
  sql::Statement statement(db_->GetUniqueStatement(kDeleteConfigSql));
  statement.BindString(0, DIPSDatabase::kPrepopulatedKey);

  if (!statement.Run()) {
    return false;
  }

  return meta_table_->SetVersionNumber(7) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(6, DIPSDatabase::kMinCompatibleSchemaVersion));
}

bool DIPSDatabaseMigrator::MigrateSchemaVersionFrom7To8() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_->HasActiveTransactions());

  return db_->Execute(
             "ALTER TABLE popups ADD COLUMN is_authentication_interaction "
             "BOOLEAN DEFAULT NULL") &&
         meta_table_->SetVersionNumber(8) &&
         meta_table_->SetCompatibleVersionNumber(
             std::min(8, DIPSDatabase::kMinCompatibleSchemaVersion));
}
