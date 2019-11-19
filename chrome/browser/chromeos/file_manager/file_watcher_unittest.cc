// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_watcher.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

using google_apis::test_util::CreateQuitCallback;
using google_apis::test_util::CreateCopyResultCallback;

class FileManagerFileWatcherTest : public testing::Test {
 public:
  // Use IO_MAINLOOP so FilePathWatcher works in the fake FILE thread, which
  // is actually shared with the main thread.
  FileManagerFileWatcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void FlushMessageLoopTasks() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FileManagerFileWatcherTest, AddAndRemoveOneExtensionId) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddExtension(kExtensionId);
  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();

  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionId, extension_ids[0]);

  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, AddAndRemoveMultipleExtensionIds) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionFooId[] = "extension-foo-id";
  const char kExtensionBarId[] = "extension-bar-id";

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddExtension(kExtensionFooId);
  file_watcher.AddExtension(kExtensionBarId);
  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();

  // The list should be sorted.
  ASSERT_EQ(2U, extension_ids.size());
  ASSERT_EQ(kExtensionBarId, extension_ids[0]);
  ASSERT_EQ(kExtensionFooId, extension_ids[1]);

  // Remove Foo. Bar should remain.
  file_watcher.RemoveExtension(kExtensionFooId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionBarId, extension_ids[0]);

  // Remove Bar. Nothing should remain.
  file_watcher.RemoveExtension(kExtensionBarId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, AddSameExtensionMultipleTimes) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  FileWatcher file_watcher(kVirtualPath);
  // Add three times.
  file_watcher.AddExtension(kExtensionId);
  file_watcher.AddExtension(kExtensionId);
  file_watcher.AddExtension(kExtensionId);

  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionId, extension_ids[0]);

  // Remove 1st time.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());

  // Remove 2nd time.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());

  // Remove 3rd time. The extension ID should be gone now.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, WatchLocalFile) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  // Create a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a callback that will run when a change is detected.
  bool on_change_error = false;
  base::FilePath changed_path;
  base::RunLoop change_run_loop;
  base::FilePathWatcher::Callback change_callback = CreateQuitCallback(
      &change_run_loop,
      CreateCopyResultCallback(&changed_path, &on_change_error));

  // Create a callback that will run when the watcher is started.
  bool watcher_created = false;
  base::RunLoop start_run_loop;
  FileWatcher::BoolCallback start_callback = CreateQuitCallback(
      &start_run_loop, CreateCopyResultCallback(&watcher_created));

  // Start watching changes in the temporary directory.
  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddExtension(kExtensionId);
  file_watcher.WatchLocalFile(temp_dir.GetPath(), change_callback,
                              std::move(start_callback));
  start_run_loop.Run();
  ASSERT_TRUE(watcher_created);

  // Create a temporary file in the temporary directory. The file watcher
  // should detect the change in the directory.
  base::FilePath temporary_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temporary_file));

  // Wait until the directory change is notified. Also flush tasks in the
  // message loop since |change_callback| can be called multiple times.
  change_run_loop.Run();
  FlushMessageLoopTasks();

  ASSERT_FALSE(on_change_error);
  ASSERT_EQ(temp_dir.GetPath().value(), changed_path.value());
}

}  // namespace
}  // namespace file_manager.
