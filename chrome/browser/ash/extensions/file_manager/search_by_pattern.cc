// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"

#include "base/files/file_enumerator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/fileapi/recent_disk_source.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace extensions {

std::string CreateFnmatchQuery(const std::string& query) {
  std::vector<std::string> query_pieces = {"*"};
  size_t sequence_start = 0;
  for (size_t i = 0; i < query.size(); ++i) {
    if (absl::ascii_isalpha(static_cast<unsigned char>(query[i]))) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }
      std::string piece("[");
      piece.resize(4);
      piece[1] = absl::ascii_tolower(static_cast<unsigned char>(query[i]));
      piece[2] = absl::ascii_toupper(static_cast<unsigned char>(query[i]));
      piece[3] = ']';
      query_pieces.push_back(std::move(piece));
      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  if (query_pieces.size() > 1) {
    query_pieces.push_back("*");
  }

  return base::StrCat(query_pieces);
}

std::vector<std::pair<base::FilePath, bool>> SearchByPattern(
    const base::FilePath& root,
    const std::vector<base::FilePath>& excluded_paths,
    const std::string& query,
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
    if (base::ranges::any_of(
            excluded_paths, [&path](const base::FilePath& excluded_path) {
              DCHECK(!path.EndsWithSeparator());
              DCHECK(!excluded_path.EndsWithSeparator());
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
