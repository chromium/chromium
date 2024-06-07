// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/database_string_table.h"

#include <stddef.h>

#include "base/strings/strcat.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace extensions {

// A target maximum size (in number of entries) for the mapping tables.  If the
// cache would grow larger than this, the size should be reduced.
static const size_t kMaximumCacheSize = 1000;

DatabaseStringTable::DatabaseStringTable(const std::string& table)
    : table_(table) {}

DatabaseStringTable::~DatabaseStringTable() {}

bool DatabaseStringTable::Initialize(sql::Database* connection) {
  if (!connection->DoesTableExist(table_.c_str())) {
    return connection->Execute(base::StrCat(
        {"CREATE TABLE ", table_,
         "(id INTEGER PRIMARY KEY, value TEXT NOT NULL);",
         "CREATE UNIQUE INDEX ", table_, "_index ON ", table_, "(value)"}));
  } else {
    return true;
  }
}

bool DatabaseStringTable::StringToInt(sql::Database* connection,
                                      const std::string& value,
                                      int64_t* id) {
  std::map<std::string, int64_t>::const_iterator lookup =
      value_to_id_.find(value);
  if (lookup != value_to_id_.end()) {
    *id = lookup->second;
    return true;
  }

  // We will be adding data to the cache below--check the cache size now and
  // reduce it if needed.
  PruneCache();

  // Operate on the assumption that the cache does a good job on
  // frequently-used strings--if there is a cache miss, first act on the
  // assumption that the string is not in the database either.
  sql::Statement update(connection->GetUniqueStatement(
      base::StrCat({"INSERT OR IGNORE INTO ", table_, "(value) VALUES (?)"})));
  update.BindString(0, value);
  if (!update.Run())
    return false;

  if (connection->GetLastChangeCount() == 1) {
    *id = connection->GetLastInsertRowId();
    id_to_value_[*id] = value;
    value_to_id_[value] = *id;
    return true;
  }

  // The specified string may have already existed in the database, in which
  // case the insert above will have been ignored.  If this happens, do a
  // lookup to find the old value.
  sql::Statement query(connection->GetUniqueStatement(
      base::StrCat({"SELECT id FROM ", table_, " WHERE value = ?"})));
  query.BindString(0, value);
  if (!query.Step())
    return false;
  *id = query.ColumnInt64(0);
  id_to_value_[*id] = value;
  value_to_id_[value] = *id;
  return true;
}

bool DatabaseStringTable::IntToString(sql::Database* connection,
                                      int64_t id,
                                      std::string* value) {
  std::map<int64_t, std::string>::const_iterator lookup = id_to_value_.find(id);
  if (lookup != id_to_value_.end()) {
    *value = lookup->second;
    return true;
  }

  // We will be adding data to the cache below--check the cache size now and
  // reduce it if needed.
  PruneCache();

  sql::Statement query(connection->GetUniqueStatement(
      base::StrCat({"SELECT value FROM ", table_, " WHERE id = ?"})));
  query.BindInt64(0, id);
  if (!query.Step())
    return false;

  *value = query.ColumnString(0);
  id_to_value_[id] = *value;
  value_to_id_[*value] = id;
  return true;
}

void DatabaseStringTable::ClearCache() {
  id_to_value_.clear();
  value_to_id_.clear();
}

void DatabaseStringTable::PruneCache() {
  if (id_to_value_.size() <= kMaximumCacheSize &&
      value_to_id_.size() <= kMaximumCacheSize)
    return;

  // TODO(mvrable): Perhaps implement a more intelligent caching policy.  For
  // now, to limit memory usage we simply clear the entire cache when it would
  // become too large.  Data will be brought back in from the database as
  // needed.
  ClearCache();
}

}  // namespace extensions
