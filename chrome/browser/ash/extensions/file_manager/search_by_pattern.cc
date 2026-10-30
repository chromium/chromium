// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/fileapi/recent_disk_source.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace extensions {

std::string CreateFnmatchQuery(std::string_view query) {
  if (query.empty()) {
    return "*";
  }

  const size_t alpha_count = std::ranges::count_if(
      query, [](unsigned char c) { return absl::ascii_isalpha(c); });
  std::string fnmatch_query;
  fnmatch_query.reserve(query.size() + 3 * alpha_count + 2);
  fnmatch_query.push_back('*');
  for (unsigned char c : query) {
    if (absl::ascii_isalpha(c)) {
      fnmatch_query.append(
          {'[', absl::ascii_tolower(c), absl::ascii_toupper(c), ']'});
    } else {
      fnmatch_query.push_back(static_cast<char>(c));
    }
  }
  fnmatch_query.push_back('*');
  return fnmatch_query;
}

std::vector<std::pair<base::FilePath, bool>> SearchByPattern(
    const base::FilePath& root,
    const std::vector<base::FilePath>& excluded_paths,
    std::string_view query,
    const base::Time& min_timestamp,
    ash::RecentSource::FileType file_type,
    size_t max_results) {
  std::vector<std::pair<base::FilePath, bool>> prefix_matches;
  std::vector<std::pair<base::FilePath, bool>> other_matches;

  base::FileEnumerator enumerator(
      root, true,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES,
      CreateFnmatchQuery(query), base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (enumerator.GetInfo().GetLastModifiedTime() < min_timestamp) {
      continue;
    }
    if (!ash::RecentDiskSource::MatchesFileType(path, file_type)) {
      continue;
    }
    // Reject files that have path in excluded paths.
    if (std::ranges::any_of(
            excluded_paths, [&path](const base::FilePath& excluded_path) {
              CHECK(!path.EndsWithSeparator(), base::NotFatalUntil::M160);
              CHECK(!excluded_path.EndsWithSeparator(),
                    base::NotFatalUntil::M160);
              return excluded_path == path || excluded_path.IsParent(path);
            })) {
      continue;
    }
    if (base::StartsWith(path.BaseName().value(), query,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      prefix_matches.emplace_back(path, enumerator.GetInfo().IsDirectory());
      if (max_results && prefix_matches.size() == max_results) {
        return prefix_matches;
      }
      continue;
    }
    if (!max_results ||
        prefix_matches.size() + other_matches.size() < max_results) {
      other_matches.emplace_back(path, enumerator.GetInfo().IsDirectory());
    }
  }
  prefix_matches.insert(
      prefix_matches.end(), other_matches.begin(),
      other_matches.begin() +
          std::min(max_results - prefix_matches.size(), other_matches.size()));

  return prefix_matches;
}

}  // namespace extensions
