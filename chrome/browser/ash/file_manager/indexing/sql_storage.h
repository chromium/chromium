// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/file_manager/indexing/term_table.h"
#include "sql/database.h"

namespace sql {
class Statement;
}  // namespace sql

namespace file_manager {

// Represents an inverted index storage implemented on top of SQL database.
// Use this in production environments. Typical use it to create an instance
// of the FileIndexService class via its factory. If you need to create it
// manually, you would need to run:
//
// base::FilePath db_path("path/to/where/db/is/stored/dbname.db");
// SqlStorage storage(db_path, "uma_unique_db_tag");
// CHECK(storage.Init());
//
// Once successfully initialized, the storage is ready to use. Use it to
// store associations between terms and files, using public method of this
// class.
class SqlStorage {
 public:
  SqlStorage(base::FilePath db_path, const std::string& uma_tag);
  ~SqlStorage();

  SqlStorage(const SqlStorage&) = delete;
  SqlStorage& operator=(const SqlStorage&) = delete;

  // Initializes the database. Returns whether or not the initialization was
  // successful. No other public method may be called until this method finishes
  // and returns true.
  bool Init();

  // Closes the database. Returns true if successful.
  bool Close();

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, we return -1, unless create is set to true.
  int64_t GetTermId(const std::string& term_bytes, bool create);

  // Removes the term ID. If the term was present in the database, it returns
  // the ID that was assigned to the term. Otherwise, it returns - 1.
  int64_t DeleteTerm(const std::string& term);

 private:
  // Error callback set on the database.
  void OnErrorCallback(int error, sql::Statement* stmt);

  // The User Metric Analysis (uma) tag for recording events related to SQL
  // storage.
  const std::string uma_tag_;

  // The full path to the database (folder and name).
  base::FilePath db_path_;

  // The actual SQL Lite database.
  sql::Database db_;

  // The table that holds mapping from tags to tag IDs.
  TermTable term_table_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
