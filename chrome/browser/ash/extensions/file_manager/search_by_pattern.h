#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SEARCH_BY_PATTERN_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SEARCH_BY_PATTERN_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Collects functions that allow us to search for files by pattern. The pattern
// matching is applied to file names only.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_source.h"

namespace extensions {

// Construct a case-insensitive fnmatch query from |query|. E.g.  for abc123,
// the result would be *[aA][bB][cC]123*.
std::string CreateFnmatchQuery(const std::string& query);

// For the given root it attempts to find files that have name matching the
// given pattern, are not in folders whose paths are anywhere on the
// excluded_paths, are modified after the given min_timestamp and are of the
// given file type. This function will return up to max_results. The results are
// returned as a vector of pairs, with the first element of the pair being the
// matched file path, and the second indicating if the file is a directory or a
// plain file.
std::vector<std::pair<base::FilePath, bool>> SearchByPattern(
    const base::FilePath& root,
    const std::vector<base::FilePath>& excluded_paths,
    const std::string& query,
    const base::Time& min_timestamp,
    ash::RecentSource::FileType file_type,
    size_t max_results);

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SEARCH_BY_PATTERN_H_
