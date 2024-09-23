// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_provider.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service_factory.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

constexpr char kFileSearchSchema[] = "file_search://";
constexpr size_t kMaxNumResults = 3;

}  // namespace

LocalImageSearchProvider::LocalImageSearchProvider(Profile* profile)
    : SearchProvider(SearchCategory::kImages),
      profile_(profile),
      thumbnail_loader_(profile) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

LocalImageSearchProvider::~LocalImageSearchProvider() = default;

ash::AppListSearchResultType LocalImageSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kImageSearch;
}

void LocalImageSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsQueryTooShort(query)) {
    // Ignore short queries, which are too noisy to be meaningful.
    return;
  }

  query_start_time_ = base::TimeTicks::Now();
  last_query_ = query;

  LocalImageSearchServiceFactory::GetForBrowserContext(profile_)->Search(
      query, kMaxNumResults,
      base::BindOnce(&LocalImageSearchProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
}

void LocalImageSearchProvider::StopQuery() {
  weak_factory_.InvalidateWeakPtrs();
  last_query_.clear();
}

void LocalImageSearchProvider::OnSearchComplete(
    const std::vector<FileSearchResult>& file_search_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "OnSearchComplete";

  SearchProvider::Results results;
  for (const auto& search_result : file_search_results) {
    DCHECK(search_result.relevance >= 0.0 && search_result.relevance <= 1.0);
    DVLOG(1) << search_result.file_path;
    results.push_back(MakeResult(search_result));
  }

  SwapResults(&results);
  UMA_HISTOGRAM_TIMES("Apps.AppList.LocalImageSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> LocalImageSearchProvider::MakeResult(
    const FileSearchResult& search_result) {
  // Use the parent directory name as details text. Take care to remove newlines
  // and handle RTL as this is displayed directly.
  std::u16string parent_dir_name = base::CollapseWhitespace(
      search_result.file_path.DirName().BaseName().LossyDisplayName(), true);
  base::i18n::SanitizeUserSuppliedString(&parent_dir_name);

  DVLOG(1) << "id: " << kFileSearchSchema + search_result.file_path.value()
           << " " << parent_dir_name << " " << last_query_
           << " rl: " << search_result.relevance;

  auto result = std::make_unique<FileResult>(
      /*id=*/kFileSearchSchema + search_result.file_path.value(),
      search_result.file_path, parent_dir_name,
      ash::AppListSearchResultType::kImageSearch,
      ash::SearchResultDisplayType::kImage, search_result.relevance,
      last_query_, FileResult::Type::kFile, profile_, &thumbnail_loader_);
  return result;
}

}  // namespace app_list
