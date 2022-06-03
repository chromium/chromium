// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_QUERY_TILES_TILE_BACKGROUND_TASK_H_
#define CHROME_BROWSER_QUERY_TILES_TILE_BACKGROUND_TASK_H_

#include "components/background_task_scheduler/background_task.h"

namespace background_task {
class BackgroundTask;
struct TaskParameters;
struct TaskInfo;
}  // namespace background_task

namespace query_tiles {

using BackgroundTask = background_task::BackgroundTask;
using TaskInfo = background_task::TaskInfo;
using TaskParameters = background_task::TaskParameters;
using TaskFinishedCallback = background_task::TaskFinishedCallback;

// Background task for query tiles.
class TileBackgroundTask : public BackgroundTask {
 public:
  TileBackgroundTask();
  ~TileBackgroundTask() override;

  TileBackgroundTask(const TileBackgroundTask& other) = delete;
  TileBackgroundTask& operator=(const TileBackgroundTask& other) = delete;

 private:
  // BackgroundTask implementation.
  void OnStartTaskInReducedMode(const TaskParameters& task_params,
                                TaskFinishedCallback callback,
                                SimpleFactoryKey* key) override;
  void OnStartTaskWithFullBrowser(
      const TaskParameters& task_params,
      TaskFinishedCallback callback,
      content::BrowserContext* browser_context) override;
  void OnFullBrowserLoaded(content::BrowserContext* browser_context) override;
  bool OnStopTask(const TaskParameters& task_params) override;

  // Function to call TileService to start fetch query tiles.
  void StartFetchTask(SimpleFactoryKey* key,
                      bool is_from_reduced_mode,
                      TaskFinishedCallback callback);

  // Cached Callback to invoke.
  TaskFinishedCallback callback_;
};

}  // namespace query_tiles

#endif  // CHROME_BROWSER_QUERY_TILES_TILE_BACKGROUND_TASK_H_
