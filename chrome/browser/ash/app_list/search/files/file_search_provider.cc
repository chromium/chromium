// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_search_provider.h"

#include <cmath>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/diacritics_checker.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

constexpr char kFileSearchSchema[] = "file_search://";
constexpr int kMaxResults = 25;
constexpr int kSearchTimeoutMs = 100;

// Construct a case-insensitive and accent-insensitive fnmatch query from
// |query|. E.g. for abc123, the result would be *[aAáàâäāåÁÀÂÄĀÅ][bB][cC]123*.
// Accent-insensitivity covers Latin-script accented characters for our
// initial implementation.
// We still honor the accented characters in |query|, and only enable
// case-insensitivity for them. E.g. ádd, the result would be *[áÁ]dd*.
std::string CreateFnmatchQuery(const std::u16string& query_input) {
  static constexpr auto conversion_map =
      base::MakeFixedFlatMap<char16_t, std::u16string_view>({
          {u'a', u"[aAáàâäāåÁÀÂÄĀÅ]"},
          {u'c', u"[cçCÇ]"},
          {u'e', u"[eEéèêëēÉÈÊËĒ]"},
          {u'i', u"[iIíìîïīÍÌÎÏĪ]"},
          {u'n', u"[nNñÑ]"},
          {u'o', u"[oOóòôöōøÓÒÔÖŌØ]"},
          {u'u', u"[uUúùûüūÚÙÛÜŪ]"},
          {u'y', u"[yYýỳŷÿȳÝỲŶŸȲ]"},
      });

  std::vector<std::u16string> query_pieces = {u"*"};
  size_t sequence_start = 0;
  const std::u16string query = base::i18n::ToLower(query_input);
  for (size_t i = 0; i < query.size(); ++i) {
    if ((query[i] >= u'a' && query[i] <= u'z') ||
        HasDiacritics(query.substr(i, 1))) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }

      auto it = conversion_map.find(query[i]);
      if (it != conversion_map.end()) {
        std::u16string piece(it->second);
        query_pieces.push_back(std::move(piece));
      } else {
        query_pieces.push_back(u"[");
        query_pieces.push_back(query.substr(i, 1));
        query_pieces.push_back(base::i18n::ToUpper(query.substr(i, 1)));
        query_pieces.push_back(u"]");
      }

      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  query_pieces.push_back(u"*");

  return base::UTF16ToUTF8(base::StrCat(query_pieces));
}

// Returns a vector of matched filepaths and a bool indicating whether or not
// the path is a directory.
std::vector<FileSearchProvider::FileInfo> SearchFilesByPattern(
    const base::FilePath& root_path,
    const std::u16string& query,
    const base::TimeTicks& query_start_time,
    const std::vector<base::FilePath> trash_paths,
    const int file_type,
    const base::span<const std::string> allowed_extensions) {
  base::FileEnumerator enumerator(
      root_path,
      /*recursive=*/true, file_type, CreateFnmatchQuery(query),
      base::FileEnumerator::FolderSearchPolicy::ALL);

  const auto time_limit = base::Milliseconds(kSearchTimeoutMs);
  bool timed_out = false;
  std::vector<FileSearchProvider::FileInfo> matched_paths;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // Exclude any paths that are parented at an enabled trash location.
    if (base::ranges::any_of(trash_paths,
                             [&path](const base::FilePath& trash_path) {
                               return trash_path.IsParent(path);
                             })) {
      continue;
    }
    // Exclude any results that are not in the allowed extensions.
    if (!allowed_extensions.empty() &&
        !base::ranges::any_of(allowed_extensions,
                              [&path](const std::string& extension) {
                                return path.MatchesFinalExtension(extension);
                              })) {
      continue;
    }

    matched_paths.emplace_back(
        path, enumerator.GetInfo().IsDirectory(),
        base::Time::FromTimeT(enumerator.GetInfo().stat().st_atime));

    if (matched_paths.size() == kMaxResults) {
      break;
    } else if (base::TimeTicks::Now() - query_start_time > time_limit) {
      timed_out = true;
      break;
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.FileSearchProvider.TimedOut", timed_out);
  return matched_paths;
}

}  // namespace

FileSearchProvider::FileSearchProvider(
    Profile* profile,
    int file_type,
    std::vector<std::string> allowed_extensions)
    : SearchProvider(SearchCategory::kFiles),
      profile_(profile),
      thumbnail_loader_(profile),
      root_path_(file_manager::util::GetMyFilesFolderForProfile(profile)),
      file_type_(file_type),
      allowed_extensions_(std::move(allowed_extensions)) {
  DCHECK(profile_);
  DCHECK(!root_path_.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FileSearchProvider::~FileSearchProvider() = default;

ash::AppListSearchResultType FileSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kFileSearch;
}

void FileSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  weak_factory_.InvalidateWeakPtrs();

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  // Generate a vector of `base::FilePath`s that can be handed to the
  // `SearchFilesByPattern`. Trash can be dynamically turned on/off via an
  // enterprise policy, so this needs to be verified on search instead of
  // precomputed.
  if (trash_paths_.empty()) {
    auto enabled_trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile_, /*base_path=*/base::FilePath());
    for (const auto& it : enabled_trash_locations) {
      trash_paths_.emplace_back(
          it.first.Append(it.second.relative_folder_path));
    }
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(SearchFilesByPattern, root_path_, query, query_start_time_,
                     (file_manager::trash::IsTrashEnabledForProfile(profile_)
                          ? trash_paths_
                          : std::vector<base::FilePath>()),
                     file_type_, allowed_extensions_),
      base::BindOnce(&FileSearchProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
}

void FileSearchProvider::StopQuery() {
  weak_factory_.InvalidateWeakPtrs();

  last_query_.clear();
  last_tokenized_query_.reset();
}

void FileSearchProvider::OnSearchComplete(
    std::vector<FileSearchProvider::FileInfo> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SearchProvider::Results results;
  for (const auto& path : paths) {
    double relevance = FileResult::CalculateRelevance(
        last_tokenized_query_, path.path, path.last_accessed);
    DCHECK((relevance >= 0.0) && (relevance <= 1.0));
    results.push_back(MakeResult(path, relevance));
  }

  SwapResults(&results);
  UMA_HISTOGRAM_TIMES("Apps.AppList.FileSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> FileSearchProvider::MakeResult(
    const FileSearchProvider::FileInfo& path,
    const double relevance) {
  const auto type = path.is_directory ? FileResult::Type::kDirectory
                                      : FileResult::Type::kFile;
  // Use the parent directory name as details text. Take care to remove newlines
  // and handle RTL as this is displayed directly.
  std::u16string parent_dir_name = base::CollapseWhitespace(
      path.path.DirName().BaseName().LossyDisplayName(), true);
  base::i18n::SanitizeUserSuppliedString(&parent_dir_name);

  auto result = std::make_unique<FileResult>(
      /*id=*/kFileSearchSchema + path.path.value(), path.path, parent_dir_name,
      ash::AppListSearchResultType::kFileSearch,
      ash::SearchResultDisplayType::kList, relevance, last_query_, type,
      profile_, &thumbnail_loader_);
  return result;
}

}  // namespace app_list
