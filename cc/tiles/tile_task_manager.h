// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_TASK_MANAGER_H_
#define CC_TILES_TILE_TASK_MANAGER_H_

#include <stddef.h>

#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/tile_task.h"

namespace cc {
// This interface provides the wrapper over TaskGraphRunner for scheduling and
// collecting tasks. The client can call CheckForCompletedTasks() at any time to
// process all completed tasks at the moment that have finished running or
// cancelled.
class CC_EXPORT TileTaskManager {
 public:
  TileTaskManager();
  virtual ~TileTaskManager();

  // Schedule running of tile tasks in |graph| and all dependencies.
  // Previously scheduled tasks that are not in |graph| will be canceled unless
  // already running. Once scheduled and if not canceled by next scheduling,
  // tasks are guaranteed to run.
  virtual void ScheduleTasks(TaskGraph* graph) = 0;

  // Check for completed tasks and call OnTaskCompleted() on them.
  virtual void CheckForCompletedTasks() = 0;

  // Shutdown after canceling all previously scheduled tasks.
  virtual void Shutdown() = 0;
};

class CC_EXPORT TileTaskManagerImpl : public TileTaskManager {
 public:
  TileTaskManagerImpl(const TileTaskManagerImpl&) = delete;
  ~TileTaskManagerImpl() override;

  TileTaskManagerImpl& operator=(const TileTaskManagerImpl&) = delete;

  static std::unique_ptr<TileTaskManagerImpl> Create(
      TaskGraphRunner* task_graph_runner);

  // Overridden from TileTaskManager:
  void ScheduleTasks(TaskGraph* graph) override;
  void CheckForCompletedTasks() override;
  void Shutdown() override;

 protected:
  explicit TileTaskManagerImpl(TaskGraphRunner* task_graph_runner);

  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
};

}  // namespace cc

#endif  // CC_TILES_TILE_TASK_MANAGER_H_
