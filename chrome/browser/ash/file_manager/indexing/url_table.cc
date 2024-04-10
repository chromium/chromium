// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/url_table.h"

namespace file_manager {

namespace {

// The statement used to create the URL table.
static constexpr char kCreateUrlTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS url_table("
        "url_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "url_spec TEXT NOT NULL)";
// clang-format on

// The statement used to delete a URL from the database by URL ID.
static constexpr char kDeleteUrlQuery[] =
    // clang-format off
     "DELETE FROM url_table WHERE url_id = ?";
// clang-format on

// The statement used fetch the ID of the URL.
static constexpr char kGetUrlIdQuery[] =
    // clang-format off
     "SELECT url_id FROM url_table WHERE url_spec = ?";
// clang-format on

// The statement used to insert a new URL into the table.
static constexpr char kInsertUrlQuery[] =
    // clang-format off
     "INSERT INTO url_table(url_spec) VALUES (?) RETURNING url_id";
// clang-format on

// The statement that creates an index on url_spec column.
static constexpr char kCreateUrlIndexQuery[] =
    // clang-format off
    "CREATE UNIQUE INDEX IF NOT EXISTS url_index ON url_table(url_spec)";
// clang-format on

}  // namespace

UrlTable::UrlTable(sql::Database* db) : TextTable(db, "url_table") {}
UrlTable::~UrlTable() = default;

int64_t UrlTable::DeleteUrl(const GURL& url) {
  DCHECK(url.is_valid());
  return DeleteValue(url.spec());
}

int64_t UrlTable::GetUrlId(const GURL& url) const {
  DCHECK(url.is_valid());
  return GetValueId(url.spec());
}

int64_t UrlTable::GetOrCreateUrlId(const GURL& url) {
  DCHECK(url.is_valid());
  return GetOrCreateValueId(url.spec());
}

std::unique_ptr<sql::Statement> UrlTable::MakeGetStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlIdQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeInsertStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertUrlQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeDeleteStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteUrlQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeCreateTableStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateUrlTableQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeCreateIndexStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateUrlIndexQuery));
}

}  // namespace file_manager
