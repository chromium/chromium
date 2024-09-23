// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SQL_DATABASE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SQL_DATABASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace sql {
class Statement;
class StatementID;
}  // namespace sql

namespace app_list {

// A database wrapper used initialize a database and maintain a table schema.
// For use on a single sequence/thread.
class SqlDatabase {
 public:
  SqlDatabase(
      const base::FilePath& path_to_db,
      const std::string& histogram_tag,
      int current_version_number,
      base::RepeatingCallback<int(SqlDatabase* db)> create_table_schema,
      base::RepeatingCallback<int(SqlDatabase* db, int current_version_number)>
          migrate_table_schema);

  ~SqlDatabase();
  SqlDatabase(const SqlDatabase&) = delete;
  SqlDatabase& operator=(const SqlDatabase&) = delete;

  // Opens or initializes the database.
  [[nodiscard]] bool Initialize();

  // Must be called in the same sequence with `Initialize()`.
  void Close();

  // Allows us to interact with the database. If the database is open, returns
  // a statement that can Bind* and Run(), otherwise nullptr.
  std::unique_ptr<sql::Statement> GetStatementForQuery(
      const sql::StatementID& sql_from_here,
      base::cstring_view query);

  base::FilePath GetPathToDb() const;

 private:
  void OnErrorCallback(int error, sql::Statement* stmt);
  bool MigrateDatabaseSchema();
  bool RazeDb();

  // The `create_table_schema` called when a new database is constructed. The
  // function must return the `current_version_number`.
  base::RepeatingCallback<int(SqlDatabase* db)> create_table_schema_;

  // The `migrate_table_schema` called when a database with an old schema is
  // open, which needs migrating. The function parameter
  // `current_version_number` is the current old version, which was loaded from
  // the SSD. The function must return the new `current_version_number`
  base::RepeatingCallback<int(SqlDatabase* db, int current_version_number)>
      migrate_table_schema_;

  // The path to the *.db file.
  const base::FilePath path_to_db_;

  // The identifying prefix for metrics.
  const std::string histogram_tag_;

  sql::Database db_;
  sql::MetaTable meta_table_;

  // The current schema version number of the database. It must be greater than
  // 1. Due to the implementation constraint of `sql::MetaTable` the version
  // cannot be zero. Yet a new metatable created for a new uninitialized
  // db has version 1 assigned. So to deduce when we need to call
  // `create_table_schema_` or `migrate_table_schema` the
  // `current_version_number_` must be greater than 1.
  const int current_version_number_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SQL_DATABASE_H_
