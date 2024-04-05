// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_TABLE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_TABLE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/file_manager/indexing/text_table.h"
#include "sql/database.h"

namespace file_manager {

// A table that maintains a mapping from a unique term ID to the term text.
// Terms for us are any pieces of text associated with some file. For example,
// if a file is labeled as "downloaded", the "downloaded" text is considered
// a term. In this table it is assigned a unique integer ID that is used
// across other tables for information retrieval.
class TermTable : public TextTable {
 public:
  // Creates a new table and passes the pointer to the SQL database to it. The
  // caller must make sure it owns both the sql::Database object and this table.
  // The caller also must make sure that the sql::Database outlives the table.
  explicit TermTable(sql::Database* db);
  ~TermTable() override;

  TermTable(const TermTable&) = delete;
  TermTable& operator=(const TermTable&) = delete;

  // Deletes the given term from the table. Returns -1, if the term was not
  // found. Otherwise, returns the ID that the term was assigned.
  int64_t DeleteTerm(const std::string& term);

  // Gets the term ID for the given term. If `create` is false, and the term
  // does not exists, this method returns -1. Otherwise it either inserts a new
  // term and returns its ID, or returns the existing term ID.
  int64_t GetTermId(const std::string& term, bool create);

 protected:
  std::unique_ptr<sql::Statement> MakeGetStatement() const override;
  std::unique_ptr<sql::Statement> MakeInsertStatement() const override;
  std::unique_ptr<sql::Statement> MakeDeleteStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateTableStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateIndexStatement() const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_TABLE_H_
