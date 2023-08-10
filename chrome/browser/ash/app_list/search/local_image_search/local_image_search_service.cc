// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"

namespace app_list {
namespace {

constexpr char kHistogramTag[] = "AnnotationStorage";
constexpr size_t kMinQueryLength = 3u;

base::FilePath ConstructPathToAnnotationDb(const Profile* const profile) {
  return profile->GetPath()
      .AppendASCII("annotation_storage")
      .AppendASCII("annotation.db");
}

std::vector<base::FilePath> GetTrashPaths(Profile* const profile) {
  std::vector<base::FilePath> excluded_paths;
  if (file_manager::trash::IsTrashEnabledForProfile(profile)) {
    const auto trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile, /*base_path=*/base::FilePath());
    for (const auto& location : trash_locations) {
      excluded_paths.emplace_back(
          location.first.Append(location.second.relative_folder_path));
    }
  }
  return excluded_paths;
}

}  // namespace

bool IsQueryTooShort(const std::u16string& query) {
  return query.size() < kMinQueryLength;
}

LocalImageSearchService::LocalImageSearchService(Profile* profile)
    : annotation_storage_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          ConstructPathToAnnotationDb(profile),
          kHistogramTag,
          std::make_unique<ImageAnnotationWorker>(
              file_manager::util::GetMyFilesFolderForProfile(profile),
              GetTrashPaths(profile),
              search_features::IsLauncherImageSearchOcrEnabled(),
              search_features::IsLauncherImageSearchIcaEnabled())) {
  DCHECK(profile);
  annotation_storage_.AsyncCall(&app_list::AnnotationStorage::Initialize);
}

LocalImageSearchService::~LocalImageSearchService() = default;

void LocalImageSearchService::Search(
    const std::u16string& query,
    base::OnceCallback<void(const std::vector<FileSearchResult>&)> callback)
    const {
  annotation_storage_.AsyncCall(&AnnotationStorage::PrefixSearch)
      .WithArgs(query)
      .Then(std::move(callback));
}

}  // namespace app_list
