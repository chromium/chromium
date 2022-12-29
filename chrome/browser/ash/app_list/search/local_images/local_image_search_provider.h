// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_

#include <string>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"

namespace app_list {

class FileResult;
class AnnotationStorage;

// TODO(b/260646344): Split into two structs. Rename to ImageSearchResult.
// Image metadata retrieved from the database. Currently, it does double duty.
// 1. It manipulates rows in the database, for which `relevance` is
// null.
// 2. It returns a result for LocalImageSearch, for which `relevance` is needed
// for ranking.
struct ImageInfo {
  // Image annotation.
  std::set<std::string> annotations;
  // Full path to the image.
  base::FilePath path;
  // Last modified time.
  base::Time last_modified;
  // Search relevance on the scale from 0-1. It represents how closely a query
  // matches the annotation.
  absl::optional<double> relevance;

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified);

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified,
            const double relevance);

  ~ImageInfo();
  ImageInfo(const ImageInfo&);
  ImageInfo& operator=(const ImageInfo&) = delete;
};

// Searches for images based on their annotations. Owns an annotation store and
// a worker for updating the store.
// TODO(b/260646344): Still in a prototype stage.
// TODO(b/260646344): Add unit tests.
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
  void OnSearchComplete(std::vector<ImageInfo> paths);
  std::unique_ptr<FileResult> MakeResult(const ImageInfo& path);

  base::TimeTicks query_start_time_;
  std::u16string last_query_;

  Profile* const profile_;
  ash::ThumbnailLoader thumbnail_loader_;
  base::FilePath root_path_;

  scoped_refptr<AnnotationStorage> annotation_storage_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalImageSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_LOCAL_IMAGE_SEARCH_PROVIDER_H_
