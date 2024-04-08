// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTEXT_DATABASE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTEXT_DATABASE_H_

#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace ash::file_system_provider {

// The persistent data store for items that are cached via an FSP mount. There
// is 1:1 mapping of `ContextDatabase` per FSP mount and when the content cache
// is removed, the database is removed with it.
class ContextDatabase {
 public:
  explicit ContextDatabase(const base::FilePath& db_path);

  ContextDatabase(const ContextDatabase&) = delete;
  ContextDatabase& operator=(const ContextDatabase&) = delete;

  ~ContextDatabase();

  // Initialize the database either in-memory (if the constructor `db_path` was
  // empty) or at the path specified by the `db_path` in the constructor.
  bool Initialize();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Remove the database and poison any subsequent requests.
  bool Raze();

  static const int kCurrentVersionNumber;
  static const int kCompatibleVersionNumber;

  const base::FilePath db_path_;
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
};

using OptionalContextDatabase = std::optional<std::unique_ptr<ContextDatabase>>;
using BoundContextDatabase =
    base::SequenceBound<std::unique_ptr<ContextDatabase>>;

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTEXT_DATABASE_H_
