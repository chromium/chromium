// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_H_

#include <string>

#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace app_list {

// Ignore short queries, which are too noisy to be meaningful.
bool IsQueryTooShort(const std::u16string& query);

// A proxy class owning annotation storage implementation.
// There can only be one AnnotationStorage instance per Profile.
class LocalImageSearchService : public KeyedService {
 public:
  explicit LocalImageSearchService(Profile* profile);
  ~LocalImageSearchService() override;

  LocalImageSearchService(const LocalImageSearchService&) = delete;
  LocalImageSearchService& operator=(const LocalImageSearchService&) = delete;

  // Asynchronous call to the backend storage.
  void Search(const std::u16string& query,
              size_t max_num_results,
              base::OnceCallback<void(const std::vector<FileSearchResult>&)>
                  callback) const;

  // Inserts the given image info into the annotation storage.
  void Insert(const ImageInfo& image_info);

 private:
  base::SequenceBound<AnnotationStorage> annotation_storage_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_SERVICE_H_
