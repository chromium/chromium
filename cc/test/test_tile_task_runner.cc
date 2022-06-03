// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_tile_task_runner.h"

#include "base/check.h"

namespace cc {

void TestTileTaskRunner::ProcessTask(TileTask* task) {
  ScheduleTask(task);
  RunTask(task);
  CompleteTask(task);
}

void TestTileTaskRunner::ScheduleTask(TileTask* task) {
  DCHECK(task);
  task->state().DidSchedule();
}

void TestTileTaskRunner::CancelTask(TileTask* task) {
  DCHECK(task);
  task->state().DidCancel();
}

void TestTileTaskRunner::RunTask(TileTask* task) {
  DCHECK(task);
  task->state().DidStart();
  task->RunOnWorkerThread();
  task->state().DidFinish();
}

void TestTileTaskRunner::CompleteTask(TileTask* task) {
  DCHECK(task);
  DCHECK(task->state().IsFinished() || task->state().IsCanceled());
  task->OnTaskCompleted();
  task->DidComplete();
}

}  // namespace cc
