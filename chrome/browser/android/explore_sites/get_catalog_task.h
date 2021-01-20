// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_CATALOG_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_CATALOG_TASK_H_

#include <vector>

#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// A task that can retrieve the catalog from the ExploreSitesStore. Also
// responsible for clearing old catalogs from the store, if created with
// |update_current| set.
//
// If |update_current| is set, this task:
// * Checks for the existence of |downloading_version| in the meta table.
// * If there is one, updates |current_version| = |downloading_version|
// * Removes all catalog data for rows not matching |current_version|.
// * Returns the current catalog.
//
// If |update_current| is not set, this task:
// * Returns any catalog with version |current_version|
// * If no |current_version|, returns catalog with version 0 (to support
//   GetCategories if necessary)
// * If no catalog with version 0, returns an empty catalog version
//
// In any case, if there is a DB error or version inconsistency, then returns
// |nullptr|.
class GetCatalogTask : public Task {
 public:
  typedef std::vector<ExploreSitesCategory> CategoryList;

  GetCatalogTask(ExploreSitesStore* store,
                 bool update_current,
                 CatalogCallback callback);
  ~GetCatalogTask() override;

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(
      std::pair<GetCatalogStatus, std::unique_ptr<CategoryList>> result);

  ExploreSitesStore* store_;  // outlives this class.

  bool update_current_;
  CatalogCallback callback_;

  base::WeakPtrFactory<GetCatalogTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetCatalogTask);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_CATALOG_TASK_H_
