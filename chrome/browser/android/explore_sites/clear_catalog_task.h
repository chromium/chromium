// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_CATALOG_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_CATALOG_TASK_H_

#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Takes a URL that the user has asked us to remove, and adds it to a blacklist
// of sites we will stop showing in Explore on Sites.
class ClearCatalogTask : public Task {
 public:
  ClearCatalogTask(ExploreSitesStore* store, BooleanCallback callback);
  ~ClearCatalogTask() override;

 private:
  // Task implementation:
  void Run() override;

  void DoneExecuting(bool result);

  ExploreSitesStore* store_;  // outlives this class.
  BooleanCallback callback_;

  base::WeakPtrFactory<ClearCatalogTask> weak_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_CATALOG_TASK_H_
