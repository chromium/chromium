// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_URL_TABLE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_URL_TABLE_H_

#include <memory>

#include "chrome/browser/ash/file_manager/indexing/text_table.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace file_manager {

// A table that maintains a mapping from a unique URL ID to the URL text.
// URLs represent location of a file. For example, for a local file we
// may have URL such as:
//
//   filesystem:chrome://file-manager/external/Downloads-user123/foo.txt
//
// This table is meant to be managed by the SqlStorage class.
class UrlTable : public TextTable {
 public:
  // Creates a new table and passes the pointer to the SQL database to it. The
  // caller must make sure it owns both the sql::Database object and this table.
  // The caller also must make sure that the sql::Database outlives the table.
  explicit UrlTable(sql::Database* db);
  ~UrlTable() override;

  UrlTable(const UrlTable&) = delete;
  UrlTable& operator=(const UrlTable&) = delete;

  // Deletes the given URL from the table. Returns -1, if the URL was not
  // found. Otherwise, returns the ID that the URL was assigned.
  int64_t DeleteUrl(const GURL& url);

  // Returns the ID for the given URL, or -1 if this URL has not been seen.
  int64_t GetUrlId(const GURL& url) const;

  // Gets or creates the URL ID for the given URL.
  int64_t GetOrCreateUrlId(const GURL& url);

 protected:
  std::unique_ptr<sql::Statement> MakeGetStatement() const override;
  std::unique_ptr<sql::Statement> MakeInsertStatement() const override;
  std::unique_ptr<sql::Statement> MakeDeleteStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateTableStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateIndexStatement() const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_URL_TABLE_H_
