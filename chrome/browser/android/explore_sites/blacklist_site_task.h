// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLACKLIST_SITE_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLACKLIST_SITE_TASK_H_

#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Takes a URL that the user has asked us to remove, and adds it to a blacklist
// of sites we will stop showing in Explore on Sites.
class BlacklistSiteTask : public Task {
 public:
  BlacklistSiteTask(ExploreSitesStore* store, std::string url);
  ~BlacklistSiteTask() override;

  bool complete() const { return complete_; }
  bool result() const { return result_; }

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(bool result);

  ExploreSitesStore* store_;  // outlives this class.
  std::string url_;

  bool complete_ = false;
  bool result_ = false;
  BooleanCallback callback_;

  base::WeakPtrFactory<BlacklistSiteTask> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLACKLIST_SITE_TASK_H_
