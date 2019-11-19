// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_INCREMENT_SHOWN_COUNT_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_INCREMENT_SHOWN_COUNT_TASK_H_

#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

class IncrementShownCountTask : public Task {
 public:
  IncrementShownCountTask(ExploreSitesStore* store, int category_id);
  ~IncrementShownCountTask() override;

  bool complete() const { return complete_; }
  bool result() const { return result_; }

 private:
  // Task impl.
  void Run() override;
  void FinishedExecuting(bool result);

  ExploreSitesStore* store_;  // outlives this class.
  int category_id_;

  bool complete_;
  bool result_;

  base::WeakPtrFactory<IncrementShownCountTask> weak_factory_{this};
};
}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_INCREMENT_SHOWN_COUNT_TASK_H_
