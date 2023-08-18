// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATIONS_TABLE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATIONS_TABLE_H_

#include <cstdint>
#include <string>

namespace app_list {

class SqlDatabase;

// A wrapper around the SQLite `annotations` table. It stores uniques annotation
// terms.
class AnnotationsTable {
 public:
  static bool Create(SqlDatabase* db);
  static bool Drop(SqlDatabase* db);
  static bool InsertOrIgnore(SqlDatabase* db, const std::string& term);
  static bool GetTermId(SqlDatabase* db,
                        const std::string& term,
                        int64_t& term_id);
  static bool Prune(SqlDatabase* db);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATIONS_TABLE_H_
