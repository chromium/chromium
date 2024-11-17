// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_DATABASE_MIGRATOR_H_
#define CHROME_BROWSER_DIPS_DIPS_DATABASE_MIGRATOR_H_

#include "sql/database.h"
#include "sql/meta_table.h"

// Returns whether the migration was successful.
bool MigrateDIPSSchemaToLatestVersion(sql::Database& db,
                                      sql::MetaTable& meta_table);

namespace internal {

class DIPSDatabaseMigrator {
 public:
  // `db` and `meta_table` must be non-null and must outlive this
  // `DIPSDatabaseMigrator` instance.
  explicit DIPSDatabaseMigrator(sql::Database* const db,
                                sql::MetaTable* const meta_table);

  // Migrates from v1 to v2 of the DIPS database schema. This migration:
  // - Makes all timestamp columns nullable instead of using base::Time() as
  // default.
  // - Replaces both the first and last stateless bounce columns to track the
  // first and last bounce times instead.
  bool MigrateSchemaVersionFrom1To2();

  // Migrates from v2 to v3 of the DIPS database schema. This migration adds two
  // extra columns for recording the first and last time a web authn assertion
  // was called.
  bool MigrateSchemaVersionFrom2To3();

  // Migrates from v3 to v4 of the DIPS database schema. This migration adds a
  // Popups table for recording popups with a current or prior user interaction.
  bool MigrateSchemaVersionFrom3To4();

  // Migrates from v4 to v5 of the DIPS database schema. This migration adds an
  // `is_current_interaction` field to the Popups table.
  bool MigrateSchemaVersionFrom4To5();

  // Migrates from v5 to v6 of the DIPS database schema. This migration adds a
  // Config table for storing key-value configuration data.
  bool MigrateSchemaVersionFrom5To6();

  // Migrates from v6 to v7 of the DIPS database schema. This migration removes
  // the deprecated config entry tracking whether the database was prepopulated.
  // Note that this is technically a data change rather than a schema change.
  // Hence the minimum compatible schema version stays the same.
  bool MigrateSchemaVersionFrom6To7();

  // Migrates from v7 to v8 of the DIPS database schema. This migration adds an
  // `is_authentication_interaction` field to the Popups table.
  bool MigrateSchemaVersionFrom7To8();

  DIPSDatabaseMigrator(const DIPSDatabaseMigrator&) = delete;
  DIPSDatabaseMigrator& operator=(const DIPSDatabaseMigrator&) = delete;

 private:
  raw_ref<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ref<sql::MetaTable> meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace internal

#endif  // CHROME_BROWSER_DIPS_DIPS_DATABASE_MIGRATOR_H_
