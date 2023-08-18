// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/file_search_result.h"

namespace app_list {

FileSearchResult::FileSearchResult(const base::FilePath& file_path,
                                   const base::Time& last_modified,
                                   double relevance)
    : file_path(file_path),
      last_modified(last_modified),
      relevance(relevance) {}

FileSearchResult::~FileSearchResult() = default;
FileSearchResult::FileSearchResult(const FileSearchResult&) = default;
FileSearchResult& FileSearchResult::operator=(const FileSearchResult&) =
    default;

}  // namespace app_list
