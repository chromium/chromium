// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_DOCUMENTS_TABLE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_DOCUMENTS_TABLE_H_

#include <cstdint>
#include <vector>

namespace base {

class FilePath;
class Time;

}  // namespace base

namespace app_list {

class SqlDatabase;

enum class DocumentType : int {
  kImage = 0,
};

// A wrapper around the SQLite `documents` table. It stores documents (files)
// metadata.
class DocumentsTable {
 public:
  static bool Create(SqlDatabase* db);
  static bool Drop(SqlDatabase* db);
  static bool InsertOrIgnore(SqlDatabase* db,
                             const base::FilePath& file_path,
                             const base::Time& last_modified_time,
                             int64_t file_size);
  static bool GetDocumentId(SqlDatabase* db,
                            const base::FilePath& file_path,
                            int64_t& document_id);
  static bool Remove(SqlDatabase* db, const base::FilePath& file_path);
  static bool GetAllFiles(SqlDatabase* db,
                          std::vector<base::FilePath>& documents);

  // Find all the files in a directory.
  static bool SearchByDirectory(SqlDatabase* db,
                                const base::FilePath& directory,
                                std::vector<base::FilePath>& matched_paths);
};

}  // namespace app_list
#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_DOCUMENTS_TABLE_H_
