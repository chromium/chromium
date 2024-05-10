// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/file_manager/indexing/augmented_term_table.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/file_info_table.h"
#include "chrome/browser/ash/file_manager/indexing/index_storage.h"
#include "chrome/browser/ash/file_manager/indexing/posting_list_table.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
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
class SqlStorage : public IndexStorage {
 public:
  SqlStorage(base::FilePath db_path, const std::string& uma_tag);
  ~SqlStorage() override;

  SqlStorage(const SqlStorage&) = delete;
  SqlStorage& operator=(const SqlStorage&) = delete;

  // Initializes the database. Returns whether or not the initialization was
  // successful. No other public method may be called until this method finishes
  // and returns true.
  [[nodiscard]] bool Init() override;

  // Closes the database. Returns true if successful.
  bool Close() override;

  // Returns the set of URL IDs associated with the given term ID.
  const std::set<int64_t> GetUrlIdsForTermId(int64_t term_id) const override;

  // Returns term IDs associated with the given URL.
  const std::set<int64_t> GetTermIdsForUrl(int64_t url_id) const override;

  // Creates an association between `term_id` and `url_id`. This
  // method is to be used when a file with the given `url_id` is known to
  // "have" the given `term_id`. The "have" here may be either the
  // file contains that term, or the user or some system assigned this term
  // to the file (labelled the file). Returns the number of added
  // associations.
  int32_t AddToPostingList(int64_t term_id, int64_t url_id) override;

  // Removes the association between `term_id` and `url_id`. This
  // method is the opposite of the AddToPostingList() and means that a file
  // with the given `url_id` no longer "has" the given `term_id`.
  // Returns the number of deleted associations.
  int32_t DeleteFromPostingList(int64_t term_id, int64_t url_id) override;

  // Returns the ID corresponding to the given token bytes. If the token bytes
  // cannot be located, we return -1.
  int64_t GetTokenId(const std::string& term_bytes) const override;

  // Returns the ID corresponding to the given token bytes. If the token bytes
  // cannot be located, a new ID is created and returned.
  int64_t GetOrCreateTokenId(const std::string& token_bytes) override;

  // Returns the ID corresponding to the given term. If the term cannot be
  // located, the method returns -1.
  int64_t GetTermId(const Term& term) const override;

  // Returns the ID corresponding to the term. If the term cannot be located,
  // a new ID is allocated and returned.
  int64_t GetOrCreateTermId(const Term& term) override;

  // Gets an ID for the given URL. Creates a new one, if this URL is seen for
  // the first time.
  int64_t GetOrCreateUrlId(const GURL& url) override;

  // Returns the ID of the given URL or -1 if it does not exists.
  int64_t GetUrlId(const GURL& url) const override;

  // Deletes the given URL and returns its ID. If the URL was not
  // seen before, this method returns -1.
  int64_t DeleteUrl(const GURL& url) override;

  // Stores the file info. The file info is stored using the ID generated from
  // the file_url. This ID is returned when the `file_info` is stored
  // successfully. Otherwise this method returns -1.
  int64_t PutFileInfo(const FileInfo& file_info) override;

  // Retrieves a FileInfo object for the give URL ID. The method returns -1,
  // if the FileInfo could not be found. Otherwise, it returns the URL ID, and
  // populates the object pointed to by the `file_info`.
  int64_t GetFileInfo(int64_t url_id, FileInfo* file_info) const override;

  // Removes the given file info from the storage. If it was not stored, this
  // method returns -1. Otherwise, it returns the `url_id`.
  int64_t DeleteFileInfo(int64_t url_id) override;

  // Miscellaneous.
  void AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                        int64_t url_id) override;
  void DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                           int64_t url_id) override;

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

  // The table that holds a mapping from tokens to token IDs.
  TokenTable token_table_;

  // The table that holds a mapping from terms to their IDs.
  TermTable term_table_;

  // The table that holds a mapping from URLs to URL IDs.
  UrlTable url_table_;

  // The table that holds a mapping from URL IDs to FileInfo objects.
  FileInfoTable file_info_table_;

  // The table that holds associations between term IDs and
  // URL IDs. It also maintains indexes that allow fast retrieval of all
  // URL IDs associated with the given term ID and all term IDs present
  // in a file with the given URL ID.
  PostingListTable posting_list_table_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
