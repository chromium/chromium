// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_RECORD_SITE_CLICK_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_RECORD_SITE_CLICK_TASK_H_

#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Record site click activity
class RecordSiteClickTask : public Task {
 public:
  RecordSiteClickTask(ExploreSitesStore* store,
                      std::string url,
                      int category_type);
  ~RecordSiteClickTask() override;

  bool complete() const { return complete_; }
  bool result() const { return result_; }

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(bool result);

  ExploreSitesStore* store_;  // outlives this class.
  std::string url_;
  int category_type_;

  bool complete_ = false;
  bool result_ = false;

  base::WeakPtrFactory<RecordSiteClickTask> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_RECORD_SITE_CLICK_TASK_H_
