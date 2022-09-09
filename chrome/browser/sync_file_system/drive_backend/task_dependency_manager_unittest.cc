// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(path) FILE_PATH_LITERAL(path)

namespace sync_file_system {
namespace drive_backend {

namespace {

base::FilePath MakePath(const base::FilePath::StringType& path) {
  return base::FilePath(path).NormalizePathSeparators();
}

bool InsertPath(TaskDependencyManager* manager,
                const std::string& app_id,
                const base::FilePath::StringType& path) {
  TaskBlocker blocker;
  blocker.app_id = app_id;
  blocker.paths.push_back(MakePath(path));
  return manager->Insert(&blocker);
}

void ErasePath(TaskDependencyManager* manager,
               const std::string& app_id,
               const base::FilePath::StringType& path) {
  TaskBlocker blocker;
  blocker.app_id = app_id;
  blocker.paths.push_back(MakePath(path));
  return manager->Erase(&blocker);
}

bool InsertExclusiveTask(TaskDependencyManager* manager) {
  TaskBlocker blocker;
  blocker.exclusive = true;
  return manager->Insert(&blocker);
}

void EraseExclusiveTask(TaskDependencyManager* manager) {
  TaskBlocker blocker;
  blocker.exclusive = true;
  manager->Erase(&blocker);
}

}  // namespace

TEST(TaskDependencyManagerTest, BasicTest) {
  TaskDependencyManager manager;
  TaskBlocker blocker;
  blocker.app_id = "app_id";
  blocker.paths.push_back(MakePath(FPL("/folder/file")));
  blocker.file_ids.push_back("file_id");
  blocker.tracker_ids.push_back(100);

  EXPECT_TRUE(manager.Insert(&blocker));
  EXPECT_FALSE(manager.Insert(&blocker));

  manager.Erase(&blocker);

  EXPECT_TRUE(manager.Insert(&blocker));

  manager.Erase(&blocker);
}

TEST(TaskDependencyManagerTest, BlocksAncestorAndDescendant) {
  TaskDependencyManager manager;

  EXPECT_TRUE(InsertPath(
      &manager, "app_id", FPL("/ancestor/parent/self/child/descendant")));
  EXPECT_FALSE(InsertPath(&manager, "app_id", FPL("/ancestor")));
  EXPECT_FALSE(InsertPath(&manager, "app_id", FPL("/ancestor/parent")));
  EXPECT_FALSE(InsertPath(&manager, "app_id", FPL("/ancestor/parent/self")));
  EXPECT_FALSE(InsertPath(
      &manager, "app_id", FPL("/ancestor/parent/self/child")));
  EXPECT_FALSE(InsertPath(
      &manager, "app_id", FPL("/ancestor/parent/self/child/descendant")));

  EXPECT_TRUE(InsertPath(
      &manager, "another_app_id", FPL("/ancestor/parent/self")));
  ErasePath(&manager, "another_app_id", FPL("/ancestor/parent/self"));

  EXPECT_TRUE(InsertPath(&manager, "app_id", FPL("/file")));
  ErasePath(&manager, "app_id", FPL("/file"));

  ErasePath(&manager, "app_id", FPL("/ancestor/parent/self/child/descendant"));
}

TEST(TaskDependencyManagerTest, ExclusiveTask) {
  TaskDependencyManager manager;

  EXPECT_TRUE(InsertPath(&manager, "app_id", FPL("/foo/bar")));
  EXPECT_FALSE(InsertExclusiveTask(&manager));
  ErasePath(&manager, "app_id", FPL("/foo/bar"));

  EXPECT_TRUE(InsertExclusiveTask(&manager));
  EXPECT_FALSE(InsertPath(&manager, "app_id", FPL("/foo/bar")));
  EraseExclusiveTask(&manager);

  EXPECT_TRUE(InsertPath(&manager, "app_id", FPL("/foo/bar")));
  ErasePath(&manager, "app_id", FPL("/foo/bar"));
}

TEST(TaskDependencyManagerTest, PermissiveTask) {
  TaskDependencyManager manager;

  EXPECT_TRUE(manager.Insert(nullptr));
  EXPECT_TRUE(InsertPath(&manager, "app_id", FPL("/foo/bar")));
  EXPECT_FALSE(InsertExclusiveTask(&manager));
  ErasePath(&manager, "app_id", FPL("/foo/bar"));

  EXPECT_FALSE(InsertExclusiveTask(&manager));
  manager.Erase(nullptr);
  EXPECT_TRUE(InsertExclusiveTask(&manager));

  EXPECT_FALSE(manager.Insert(nullptr));

  EraseExclusiveTask(&manager);
}

}  // namespace drive_backend
}  // namespace sync_file_system
