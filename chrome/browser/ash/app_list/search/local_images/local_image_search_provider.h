// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"

namespace app_list {

class FileResult;
class AnnotationStorage;
struct FileSearchResult;

// Searches for images based on their annotations. Owns an annotation store and
// a worker for updating the store.
// TODO(b/260646344): Still in a prototype stage.
class LocalImageSearchProvider : public SearchProvider {
 public:
  explicit LocalImageSearchProvider(Profile* profile);
  ~LocalImageSearchProvider() override;

  LocalImageSearchProvider(const LocalImageSearchProvider&) = delete;
  LocalImageSearchProvider& operator=(const LocalImageSearchProvider&) = delete;

  // SearchProvider overrides:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;
  void StopQuery() override;

 private:
  void OnSearchComplete(
      const std::map<base::FilePath, FileSearchResult>& file_search_results);
  std::unique_ptr<FileResult> MakeResult(const FileSearchResult& search_result,
                                         const base::FilePath& path);

  base::TimeTicks query_start_time_;
  std::u16string last_query_;

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  ash::ThumbnailLoader thumbnail_loader_;
  base::FilePath root_path_;

  base::SequenceBound<AnnotationStorage> annotation_storage_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalImageSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_
