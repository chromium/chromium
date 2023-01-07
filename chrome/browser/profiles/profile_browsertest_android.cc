// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileDeleteMediaBrowserTest : public AndroidBrowserTest {
 protected:
  void SetUp() override {
    // This needs to happen before AndroidBrowserTest::SetUp(), since that
    // invokes deletion. Luckily on Android chrome::GetUserCacheDirectory()
    // doesn't actually look at its input. (This would be cleaner as a PRE_
    // test, but that doesn't appear to be supported here).
    chrome::GetUserCacheDirectory(base::FilePath(), &cache_base_);
    base::FilePath media_cache_path =
        cache_base_.Append(chrome::kMediaCacheDirname);

    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(media_cache_path));
    std::string data = "foo";
    base::WriteFile(media_cache_path.AppendASCII("foo"), data);

    AndroidBrowserTest::SetUp();
  }

  base::FilePath cache_base_;
};

namespace {

// Watches for the destruction of the specified path (Which, in the tests that
// use it, is typically a directory), and expects the parent directory not to be
// deleted.
//
// The public methods are called on the UI thread, the private ones called on a
// separate SequencedTaskRunner.
class FileDestructionWatcher {
 public:
  explicit FileDestructionWatcher(const base::FilePath& watched_file_path)
      : watched_file_path_(watched_file_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  FileDestructionWatcher(const FileDestructionWatcher&) = delete;
  FileDestructionWatcher& operator=(const FileDestructionWatcher&) = delete;

  void WaitForDestruction() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!watcher_);
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&FileDestructionWatcher::StartWatchingPath,
                                  base::Unretained(this)));
    run_loop_.Run();
    // The watcher should be destroyed before quitting the run loop, once the
    // file has been destroyed.
    DCHECK(!watcher_);

    // Double check that the file was destroyed, and that the parent directory
    // was not.
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(watched_file_path_));
    EXPECT_TRUE(base::PathExists(watched_file_path_.DirName()));
  }

 private:
  void StartWatchingPath() {
    DCHECK(!watcher_);
    watcher_ = std::make_unique<base::FilePathWatcher>();
    // Start watching before checking if the file exists, as the file could be
    // destroyed between the existence check and when we start watching, if the
    // order were reversed.
    EXPECT_TRUE(watcher_->Watch(
        watched_file_path_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindRepeating(&FileDestructionWatcher::OnPathChanged,
                            base::Unretained(this))));
    CheckIfPathExists();
  }

  void OnPathChanged(const base::FilePath& path, bool error) {
    EXPECT_EQ(watched_file_path_, path);
    EXPECT_FALSE(error);
    CheckIfPathExists();
  }

  // Checks if the path exists, and if so, destroys the watcher and quits
  // |run_loop_|.
  void CheckIfPathExists() {
    if (!base::PathExists(watched_file_path_)) {
      watcher_.reset();
      run_loop_.Quit();
      return;
    }
  }

  base::RunLoop run_loop_;
  const base::FilePath watched_file_path_;

  // Created and destroyed off of the UI thread, on the sequence used to watch
  // for changes.
  std::unique_ptr<base::FilePathWatcher> watcher_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfileDeleteMediaBrowserTest, DeleteMediaCache) {
  // Make sure the legacy media cache directory (created in SetUp) gets deleted
  // properly.
  base::FilePath cache_base;
  chrome::GetUserCacheDirectory(
      TabModelList::models()[0]->GetProfile()->GetPath(), &cache_base);

  // |cache_base_| computation in SetUp() makes assumptions on implementation
  // details to be able to run that early, so verify its result is sane.
  EXPECT_EQ(cache_base, cache_base_);
  base::FilePath media_cache_path =
      cache_base.Append(chrome::kMediaCacheDirname);

  base::ScopedAllowBlockingForTesting allow_blocking;
  FileDestructionWatcher destruction_watcher(media_cache_path);
  destruction_watcher.WaitForDestruction();
}
