// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_IMAGES_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_IMAGES_TASK_H_

#include <vector>

#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// A task that can retrieve images from the catalog from the ExploreSitesStore.
//
// Creators can specify either a category with a maximum number of images, or
// a site (for which only 0 or 1 images will be returned).
//
// Does not do any version checking, since site_id and category_id are version-
// specific.
class GetImagesTask : public Task {
 public:
  enum class DataType { kCategory, kSite, kSummary };

  GetImagesTask(ExploreSitesStore* store,
                int category_id,
                int max_images,
                EncodedImageListCallback callback);

  GetImagesTask(ExploreSitesStore* store,
                int site_id,
                EncodedImageListCallback callback);

  GetImagesTask(ExploreSitesStore* store,
                DataType data_type,
                int max_images,
                EncodedImageListCallback callback);

  ~GetImagesTask() override;

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(EncodedImageList images);

  ExploreSitesStore* store_;  // outlives this class.

  DataType data_type_;
  int id_;
  int max_results_;

  EncodedImageListCallback callback_;

  base::WeakPtrFactory<GetImagesTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetImagesTask);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_IMAGES_TASK_H_
