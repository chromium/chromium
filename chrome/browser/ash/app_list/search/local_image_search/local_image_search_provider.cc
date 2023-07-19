// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_provider.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

constexpr char kFileSearchSchema[] = "file_search://";
constexpr char kHistogramTag[] = "AnnotationStorage";
constexpr size_t kMinQueryLength = 3u;

base::FilePath ConstructPathToAnnotationDb(const Profile* const profile) {
  return profile->GetPath()
      .AppendASCII("annotation_storage")
      .AppendASCII("annotation.db");
}

}  // namespace

LocalImageSearchProvider::LocalImageSearchProvider(Profile* profile)
    : profile_(profile),
      thumbnail_loader_(profile),
      root_path_(file_manager::util::GetMyFilesFolderForProfile(profile)),
      annotation_storage_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          ConstructPathToAnnotationDb(profile_),
          kHistogramTag,
          std::make_unique<ImageAnnotationWorker>(
              root_path_,
              search_features::IsLauncherImageSearchOcrEnabled(),
              search_features::IsLauncherImageSearchIcaEnabled())) {
  DCHECK(profile_);
  DCHECK(!root_path_.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  annotation_storage_.AsyncCall(&AnnotationStorage::Initialize);
}

LocalImageSearchProvider::~LocalImageSearchProvider() = default;

ash::AppListSearchResultType LocalImageSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kImageSearch;
}

void LocalImageSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (query.size() < kMinQueryLength) {
    // Ignore short queries, which too noisy to be meaningful.
    return;
  }

  query_start_time_ = base::TimeTicks::Now();
  last_query_ = query;

  annotation_storage_.AsyncCall(&AnnotationStorage::Search)
      .WithArgs(query)
      .Then(base::BindOnce(&LocalImageSearchProvider::OnSearchComplete,
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
  // TODO(b/260646344): add to UMA, latency
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
      last_query_, FileResult::Type::kFile, profile_);
  result->RequestThumbnail(&thumbnail_loader_);
  return result;
}

}  // namespace app_list
