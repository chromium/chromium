// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_VERSION_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_VERSION_TASK_H_

#include "base/callback.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Fetches the version of the catalog that we have currently. If we are in
// the state where the catalog is already downloaded but not yet in use, we
// return the "downloading" catalog version.
class GetVersionTask : public Task {
 public:
  GetVersionTask(ExploreSitesStore* store,
                 base::OnceCallback<void(std::string)> callback);
  ~GetVersionTask() override;

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(std::string result);

  ExploreSitesStore* store_;  // outlives this class.
  base::OnceCallback<void(std::string)> callback_;
  base::WeakPtrFactory<GetVersionTask> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_GET_VERSION_TASK_H_
