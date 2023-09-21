// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_INVERTED_INDEX_TABLE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_INVERTED_INDEX_TABLE_H_

#include <cstdint>

namespace base {

class FilePath;

}

namespace app_list {

class SqlDatabase;

// A wrapper around the SQLite `inverted_index` table. It is backend for
// the inverted index search.
class InvertedIndexTable {
 public:
  static bool Create(SqlDatabase* db);
  static bool Drop(SqlDatabase* db);
  static bool Insert(SqlDatabase* db, int64_t term_id, int64_t document_id);
  static bool Remove(SqlDatabase* db, const base::FilePath& file_path);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_INVERTED_INDEX_TABLE_H_
