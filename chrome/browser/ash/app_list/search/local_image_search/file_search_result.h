// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_FILE_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_FILE_SEARCH_RESULT_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace app_list {

// A search result with `relevance` to the supplied query.
struct FileSearchResult {
  // The full path to the file.
  base::FilePath file_path;
  // The file's last modified time.
  base::Time last_modified;
  // The file's relevance on the scale from 0-1. It represents how closely a
  // query matches the file's annotation.
  double relevance;

  FileSearchResult(const base::FilePath& file_path,
                   const base::Time& last_modified,
                   double relevance);

  ~FileSearchResult();
  FileSearchResult(const FileSearchResult&);
  FileSearchResult& operator=(const FileSearchResult&);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_FILE_SEARCH_RESULT_H_
