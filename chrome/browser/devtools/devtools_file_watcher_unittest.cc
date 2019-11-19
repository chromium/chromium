// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsFileWatcherTest : public testing::Test {
 public:
  void Callback(const std::vector<std::string>& changed_paths,
                const std::vector<std::string>& added_paths,
                const std::vector<std::string>& removed_paths) {
    for (auto& p : changed_paths)
      expected_changed_paths_.erase(p);
    for (auto& p : added_paths)
      ASSERT_EQ(1ul, expected_added_paths_.erase(p));
    for (auto& p : removed_paths)
      ASSERT_EQ(1ul, expected_removed_paths_.erase(p));
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base_path_ = temp_dir_.GetPath();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath base_path_;
  base::test::TaskEnvironment task_environment_;
  bool done_flag_ = false;

  std::set<std::string> expected_changed_paths_;
  std::set<std::string> expected_added_paths_;
  std::set<std::string> expected_removed_paths_;
};

TEST_F(DevToolsFileWatcherTest, BasicUsage) {
  std::unique_ptr<DevToolsFileWatcher, DevToolsFileWatcher::Deleter> watcher(
      new DevToolsFileWatcher(base::Bind(&DevToolsFileWatcherTest::Callback,
                                         base::Unretained(this)),
                              base::SequencedTaskRunnerHandle::Get()));

  base::FilePath changed_path = base_path_.Append(FILE_PATH_LITERAL("file1"));
  base::WriteFile(changed_path, "test", 4);

  watcher->AddWatch(base_path_);
  expected_changed_paths_.insert(changed_path.AsUTF8Unsafe());

  while (!expected_changed_paths_.empty()) {
    task_environment_.RunUntilIdle();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    // Just for the first operation, repeat it until we get the callback, as
    // watcher may take some time to start on another thread.
    base::WriteFile(changed_path, "test", 4);
  }

  base::FilePath added_path = base_path_.Append(FILE_PATH_LITERAL("file2"));
  expected_added_paths_.insert(added_path.AsUTF8Unsafe());
  base::WriteFile(added_path, "test", 4);
  while (!expected_added_paths_.empty()) {
    task_environment_.RunUntilIdle();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  expected_removed_paths_.insert(added_path.AsUTF8Unsafe());
  base::DeleteFile(added_path, false);
  while (!expected_removed_paths_.empty()) {
    task_environment_.RunUntilIdle();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
}
