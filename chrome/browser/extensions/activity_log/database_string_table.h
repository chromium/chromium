// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_DATABASE_STRING_TABLE_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_DATABASE_STRING_TABLE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/gtest_prod_util.h"

namespace sql {
class Database;
}  // namespace sql

namespace extensions {

// A class for maintaining a persistent mapping between strings and integers.
// This is used to help compress the contents of the activity log database on
// disk by replacing repeated strings by smaller integers.
//
// The mapping from integers to strings is maintained in a database table, but
// the mapping is also cached in memory.
//
// The database table used to store the strings is configurable, but its layout
// is fixed: it always consists of just two columns, "id" and "value".
//
// All calls to DatabaseStringTable must occur on the database thread.
class DatabaseStringTable {
 public:
  explicit DatabaseStringTable(const std::string& table);

  DatabaseStringTable(const DatabaseStringTable&) = delete;
  DatabaseStringTable& operator=(const DatabaseStringTable&) = delete;

  ~DatabaseStringTable();

  // Initialize the database table.  This will create the table if it does not
  // exist.  Returns true on success; false on error.
  bool Initialize(sql::Database* connection);

  // Interns a string in the database table and sets *id to the corresponding
  // integer.  If the string already exists, the existing number is returned;
  // otherwise, new database row is inserted with the new string.  Returns true
  // on success and false on database error.
  bool StringToInt(sql::Database* connection,
                   const std::string& value,
                   int64_t* id);

  // Looks up an integer value and converts it to a string (which is stored in
  // *value).  Returns true on success.  A false return does not necessarily
  // indicate a database error; it might simply be that the value cannot be
  // found.
  bool IntToString(sql::Database* connection, int64_t id, std::string* value);

  // Clears the in-memory cache; this should be called if the underlying
  // database table has been manipulated and the cache may be stale.
  void ClearCache();

 private:
  // Reduces the size of the cache if too many entries are held in it.
  void PruneCache();

  // In-memory caches of recently accessed values.
  std::map<int64_t, std::string> id_to_value_;
  std::map<std::string, int64_t> value_to_id_;

  // The name of the database table where the mapping is stored.
  std::string table_;

  FRIEND_TEST_ALL_PREFIXES(DatabaseStringTableTest, Prune);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_DATABASE_STRING_TABLE_H_
