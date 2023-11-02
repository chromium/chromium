// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_BACKGROUND_REFRESH_TASK_H_
#define CHROME_BROWSER_FEED_ANDROID_BACKGROUND_REFRESH_TASK_H_

#include "components/background_task_scheduler/background_task.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

class BackgroundRefreshTask : public background_task::BackgroundTask {
 public:
  explicit BackgroundRefreshTask(RefreshTaskId task_id);
  ~BackgroundRefreshTask() override;

  BackgroundRefreshTask(const BackgroundRefreshTask& other) = delete;
  BackgroundRefreshTask& operator=(const BackgroundRefreshTask& other) = delete;

 private:
  // background_task::BackgroundTask.
  void OnStartTaskInReducedMode(
      const background_task::TaskParameters& task_params,
      background_task::TaskFinishedCallback callback,
      SimpleFactoryKey* key) override;
  void OnStartTaskWithFullBrowser(
      const background_task::TaskParameters& task_params,
      background_task::TaskFinishedCallback callback,
      content::BrowserContext* browser_context) override;
  void OnFullBrowserLoaded(content::BrowserContext* browser_context) override;
  bool OnStopTask(const background_task::TaskParameters& task_params) override;

  void Run(background_task::TaskFinishedCallback callback,
           content::BrowserContext* browser_context);

  RefreshTaskId task_id_;
  // Callback saved from |OnStartTaskInReducedMode()|.
  background_task::TaskFinishedCallback callback_;
};

}  // namespace feed
#endif  // CHROME_BROWSER_FEED_ANDROID_BACKGROUND_REFRESH_TASK_H_
