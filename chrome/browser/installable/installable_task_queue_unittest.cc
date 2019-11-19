// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_task_queue.h"

#include "chrome/browser/installable/installable_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

// A POD struct which holds booleans for creating and comparing against
// a (move-only) InstallableTask.
struct TaskParams {
  bool valid_manifest = false;
  bool has_worker = false;
  bool valid_primary_icon = false;
  bool valid_badge_icon = false;
};

// Constructs an InstallableTask, with the supplied bools stored in it.
InstallableTask CreateTask(const TaskParams& params) {
  InstallableTask task;
  task.params.valid_manifest = params.valid_manifest;
  task.params.has_worker = params.has_worker;
  task.params.valid_primary_icon = params.valid_primary_icon;
  task.params.valid_badge_icon = params.valid_badge_icon;
  return task;
}

bool IsEqual(const TaskParams& params, const InstallableTask& task) {
  return task.params.valid_manifest == params.valid_manifest &&
         task.params.has_worker == params.has_worker &&
         task.params.valid_primary_icon == params.valid_primary_icon &&
         task.params.valid_badge_icon == params.valid_badge_icon;
}

class InstallableTaskQueueUnitTest : public testing::Test {};

TEST_F(InstallableTaskQueueUnitTest, PausingMakesNextTaskAvailable) {
  InstallableTaskQueue task_queue;
  TaskParams task1 = {false, false, false, false};
  TaskParams task2 = {true, true, true, true};

  EXPECT_FALSE(task_queue.HasCurrent());
  EXPECT_FALSE(task_queue.HasPaused());

  task_queue.Add(CreateTask(task1));
  task_queue.Add(CreateTask(task2));

  EXPECT_TRUE(task_queue.HasCurrent());
  EXPECT_FALSE(task_queue.HasPaused());
  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));

  // There is another task in the main queue, so it becomes current.
  task_queue.PauseCurrent();
  EXPECT_TRUE(task_queue.HasCurrent());
  EXPECT_TRUE(task_queue.HasPaused());
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));

  task_queue.Reset();
  EXPECT_FALSE(task_queue.HasCurrent());
  EXPECT_FALSE(task_queue.HasPaused());
}

TEST_F(InstallableTaskQueueUnitTest, PausedTaskCanBeRetrieved) {
  InstallableTaskQueue task_queue;
  TaskParams task1 = {false, false, false, false};
  TaskParams task2 = {true, true, true, true};

  task_queue.Add(CreateTask(task1));
  task_queue.Add(CreateTask(task2));

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  task_queue.PauseCurrent();
  EXPECT_TRUE(task_queue.HasCurrent());
  EXPECT_TRUE(task_queue.HasPaused());
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  task_queue.UnpauseAll();

  // We've unpaused "1", but "2" is still current.
  EXPECT_TRUE(task_queue.HasCurrent());
  EXPECT_FALSE(task_queue.HasPaused());
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  task_queue.Next();
  EXPECT_TRUE(task_queue.HasCurrent());
  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));

  task_queue.Reset();
  EXPECT_FALSE(task_queue.HasCurrent());
  EXPECT_FALSE(task_queue.HasPaused());
}

TEST_F(InstallableTaskQueueUnitTest, NextDiscardsTask) {
  InstallableTaskQueue task_queue;
  TaskParams task1 = {false, false, false, false};
  TaskParams task2 = {true, true, true, true};

  task_queue.Add(CreateTask(task1));
  task_queue.Add(CreateTask(task2));

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  task_queue.Next();
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  // Next() does not pause "1"; it just drops it, so there is nothing to
  // unpause.
  task_queue.UnpauseAll();
  // "2" is still current.
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  // Unpausing does not retrieve "1"; it's gone forever.
  task_queue.Next();
  EXPECT_FALSE(task_queue.HasCurrent());
}
