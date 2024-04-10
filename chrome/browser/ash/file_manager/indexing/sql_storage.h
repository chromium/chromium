// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/file_info_table.h"
#include "chrome/browser/ash/file_manager/indexing/term_table.h"
#include "chrome/browser/ash/file_manager/indexing/url_table.h"
#include "sql/database.h"
#include "url/gurl.h"

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

  // Gets an ID for the given URL. Creates a new one, if this URL is seen for
  // the first time.
  int64_t GetOrCreateUrlId(const GURL& url);

  // Returns the ID of the given URL or -1 if it does not exists.
  int64_t GetUrlId(const GURL& url);

  // Deletes the given URL and returns its ID. If the URL was not
  // seen before, this method returns -1.
  int64_t DeleteUrl(const GURL& url);

  // Stores the gile info. The file info is stored using the ID generated from
  // the file_url. This ID is returned when the `file_info` is stored
  // successfully. Otherwise this method returns -1.
  int64_t PutFileInfo(const FileInfo& file_info);

  // Retrieves a FileInfo object for the give URL ID. The method returns false,
  // if the FileInfo could not be found. Otherwise, it returns true, and
  // populates the object pointed to by the `file_info`.
  int64_t GetFileInfo(const GURL& url, FileInfo* file_info);

  // Removes the given file info from the storage. If it was not stored, this
  // method returns -1. Otherwise, it returns the ID of the `url` parameter.
  int64_t DeleteFileInfo(const GURL& url);

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

  // The table that holds a mapping from tags to tag IDs.
  TermTable term_table_;

  // The table that holds a mapping from URLs to URL IDs.
  UrlTable url_table_;

  // The table that holds a mapping from URL IDs to FileInfo objects.
  FileInfoTable file_info_table_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
