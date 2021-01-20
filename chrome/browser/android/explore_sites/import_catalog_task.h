// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMPORT_CATALOG_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMPORT_CATALOG_TASK_H_

#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Takes a Catalog proto and adds records to the store as the "downloading"
// version.  Another task promotes "downloading" to "current".
// It has the following behavior in edge cases:
// * If the timestamp matches the "current" version, it does nothing. This
//   prevents stomping on the currently viewable catalog.
// * If the timestamp matches the "downloading" version, it overwrites it,
//   since that version is not yet viewable by the user.
class ImportCatalogTask : public Task {
 public:
  ImportCatalogTask(ExploreSitesStore* store,
                    std::string version_token,
                    std::unique_ptr<Catalog> catalog_proto,
                    BooleanCallback callback);
  ~ImportCatalogTask() override;

  bool complete() const { return complete_; }
  bool result() const { return result_; }

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(bool result);

  ExploreSitesStore* store_;  // outlives this class.
  std::string version_token_;
  std::unique_ptr<Catalog> catalog_proto_;

  bool complete_ = false;
  bool result_ = false;
  BooleanCallback callback_;

  base::WeakPtrFactory<ImportCatalogTask> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_IMPORT_CATALOG_TASK_H_
