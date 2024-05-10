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

// A table that maintains a mapping from a unique token ID to the token text.
// Tokens for us are any pieces of text associated with some file. For example,
// if a file is labeled as "downloaded", the "downloaded" text is considered
// a token. In this table it is assigned a unique integer ID that is used
// across other tables for information retrieval.
class TokenTable : public TextTable {
 public:
  // Creates a new table and passes the pointer to the SQL database to it. The
  // caller must make sure it owns both the sql::Database object and this table.
  // The caller also must make sure that the sql::Database outlives the table.
  explicit TokenTable(sql::Database* db);
  ~TokenTable() override;

  TokenTable(const TokenTable&) = delete;
  TokenTable& operator=(const TokenTable&) = delete;

  // Deletes the given token from the table. Returns -1, if the term was not
  // found. Otherwise, returns the ID that the term was assigned.
  int64_t DeleteToken(const std::string& token_bytes);

  // Gets the term ID for the given token bytes. If the term cannot be found,
  // this method returns -1.
  int64_t GetTokenId(const std::string& token_bytes) const;

  // For the given `token_id` attempts to find the corresponding token value.
  // If one cannot be found, returns -1. Otherwise returns `token_id` and fills
  // the token with the found value.
  int64_t GetToken(int64_t term_id, std::string* token_bytes) const;

  // Gets or creates the unique token ID for the given token bytes.
  int64_t GetOrCreateTokenId(const std::string& term_bytes);

 protected:
  std::unique_ptr<sql::Statement> MakeGetValueIdStatement() const override;
  std::unique_ptr<sql::Statement> MakeGetValueStatement() const override;
  std::unique_ptr<sql::Statement> MakeInsertStatement() const override;
  std::unique_ptr<sql::Statement> MakeDeleteStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateTableStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateIndexStatement() const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_TABLE_H_
