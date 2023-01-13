// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>
#elif BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/file_path_watcher_inotify.h"
#include "base/format_macros.h"
#endif

namespace base {

namespace {

class TestDelegate;

// Aggregates notifications from the test delegates and breaks the run loop
// the test thread is waiting on once they all came in.
class NotificationCollector
    : public base::RefCountedThreadSafe<NotificationCollector> {
 public:
  NotificationCollector()
      : task_runner_(SingleThreadTaskRunner::GetCurrentDefault()) {}

  // Called from the file thread by the delegates.
  void OnChange(TestDelegate* delegate) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NotificationCollector::RecordChange, this,
                                  base::Unretained(delegate)));
  }

  void Register(TestDelegate* delegate) {
    delegates_.insert(delegate);
  }

  void Reset(base::OnceClosure signal_closure) {
    signal_closure_ = std::move(signal_closure);
    signaled_.clear();
  }

  bool Success() {
    return signaled_ == delegates_;
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationCollector>;
  ~NotificationCollector() = default;

  void RecordChange(TestDelegate* delegate) {
    // Warning: |delegate| is Unretained. Do not dereference.
    ASSERT_TRUE(task_runner_->BelongsToCurrentThread());
    ASSERT_TRUE(delegates_.count(delegate));
    signaled_.insert(delegate);

    // Check whether all delegates have been signaled.
    if (signal_closure_ && signaled_ == delegates_)
      std::move(signal_closure_).Run();
  }

  // Set of registered delegates.
  std::set<TestDelegate*> delegates_;

  // Set of signaled delegates.
  std::set<TestDelegate*> signaled_;

  // The loop we should break after all delegates signaled.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Closure to run when all delegates have signaled.
  base::OnceClosure signal_closure_;
};

class TestDelegateBase : public SupportsWeakPtr<TestDelegateBase> {
 public:
  TestDelegateBase() = default;
  TestDelegateBase(const TestDelegateBase&) = delete;
  TestDelegateBase& operator=(const TestDelegateBase&) = delete;
  virtual ~TestDelegateBase() = default;

  virtual void OnFileChanged(const FilePath& path, bool error) = 0;
};

// A mock class for testing. Gmock is not appropriate because it is not
// thread-safe for setting expectations. Thus the test code cannot safely
// reset expectations while the file watcher is running.
// Instead, TestDelegate gets the notifications from FilePathWatcher and uses
// NotificationCollector to aggregate the results.
class TestDelegate : public TestDelegateBase {
 public:
  explicit TestDelegate(NotificationCollector* collector)
      : collector_(collector) {
    collector_->Register(this);
  }
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  // Configure this delegate so that it expects an error.
  void set_expect_error() { expect_error_ = true; }
  // Returns observed paths for each invocation of OnFileChanged.
  std::vector<FilePath> get_observed_paths() const { return observed_paths_; }

  // TestDelegateBase:
  void OnFileChanged(const FilePath& path, bool error) override {
    observed_paths_.push_back(path);

    if (error != expect_error_) {
      ADD_FAILURE() << "Unexpected change for \"" << path
                    << "\" with |error| = " << (error ? "true" : "false");
    } else {
      collector_->OnChange(this);
    }
  }

 private:
  scoped_refptr<NotificationCollector> collector_;
  bool expect_error_ = false;
  std::vector<FilePath> observed_paths_;
};

class FilePathWatcherTest : public testing::Test {
 public:
  FilePathWatcherTest()
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      : task_environment_(test::TaskEnvironment::MainThreadType::IO)
#endif
  {
  }

  FilePathWatcherTest(const FilePathWatcherTest&) = delete;
  FilePathWatcherTest& operator=(const FilePathWatcherTest&) = delete;
  ~FilePathWatcherTest() override = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    // Watching files is only permitted when all parent directories are
    // accessible, which is not the case for the default temp directory
    // on Android which is under /data/data.  Use /sdcard instead.
    // TODO(pauljensen): Remove this when crbug.com/475568 is fixed.
    FilePath parent_dir;
    ASSERT_TRUE(android::GetExternalStorageDirectory(&parent_dir));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(parent_dir));
#else   // BUILDFLAG(IS_ANDROID)
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#endif  // BUILDFLAG(IS_ANDROID)
    collector_ = new NotificationCollector();
  }

  void TearDown() override { RunLoop().RunUntilIdle(); }

  FilePath test_file() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest");
  }

  FilePath test_link() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest.lnk");
  }

  [[nodiscard]] bool SetupWatch(const FilePath& target,
                                FilePathWatcher* watcher,
                                TestDelegateBase* delegate,
                                FilePathWatcher::Type watch_type);

  [[nodiscard]] bool SetupWatchWithOptions(
      const FilePath& target,
      FilePathWatcher* watcher,
      TestDelegateBase* delegate,
      FilePathWatcher::WatchOptions watch_options);

  [[nodiscard]] bool WaitForEvent() {
    return WaitForEventWithTimeout(TestTimeouts::action_timeout());
  }

  [[nodiscard]] bool WaitForEventWithTimeout(TimeDelta timeout) {
    RunLoop run_loop;
    collector_->Reset(run_loop.QuitClosure());

    // Make sure we timeout if we don't get notified.
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
    return collector_->Success();
  }

  NotificationCollector* collector() { return collector_.get(); }

  test::TaskEnvironment task_environment_;

  ScopedTempDir temp_dir_;
  scoped_refptr<NotificationCollector> collector_;
};

bool FilePathWatcherTest::SetupWatch(const FilePath& target,
                                     FilePathWatcher* watcher,
                                     TestDelegateBase* delegate,
                                     FilePathWatcher::Type watch_type) {
  return watcher->Watch(target, watch_type,
                        base::BindRepeating(&TestDelegateBase::OnFileChanged,
                                            delegate->AsWeakPtr()));
}

bool FilePathWatcherTest::SetupWatchWithOptions(
    const FilePath& target,
    FilePathWatcher* watcher,
    TestDelegateBase* delegate,
    FilePathWatcher::WatchOptions watch_options) {
  return watcher->WatchWithOptions(
      target, watch_options,
      base::BindRepeating(&TestDelegateBase::OnFileChanged,
                          delegate->AsWeakPtr()));
}

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, NewFile) {
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, ModifiedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that moving the file into place is caught.
#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_MovedFile DISABLED_MovedFile
#else
#define MAYBE_MovedFile MovedFile
#endif
TEST_F(FilePathWatcherTest, MAYBE_MovedFile) {
  FilePath source_file(temp_dir_.GetPath().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(base::Move(source_file, test_file()));
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_DeletedFile DISABLED_DeletedFile
#else
#define MAYBE_DeletedFile DeletedFile
#endif
TEST_F(FilePathWatcherTest, MAYBE_DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is deleted.
  base::DeleteFile(test_file());
  ASSERT_TRUE(WaitForEvent());
}

// Used by the DeleteDuringNotify test below.
// Deletes the FilePathWatcher when it's notified.
class Deleter : public TestDelegateBase {
 public:
  explicit Deleter(base::OnceClosure done_closure)
      : watcher_(std::make_unique<FilePathWatcher>()),
        done_closure_(std::move(done_closure)) {}
  Deleter(const Deleter&) = delete;
  Deleter& operator=(const Deleter&) = delete;
  ~Deleter() override = default;

  void OnFileChanged(const FilePath&, bool) override {
    watcher_.reset();
    std::move(done_closure_).Run();
  }

  FilePathWatcher* watcher() const { return watcher_.get(); }

 private:
  std::unique_ptr<FilePathWatcher> watcher_;
  base::OnceClosure done_closure_;
};

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  base::RunLoop run_loop;
  Deleter deleter(run_loop.QuitClosure());
  ASSERT_TRUE(SetupWatch(test_file(), deleter.watcher(), &deleter,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  run_loop.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher() == nullptr);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
TEST_F(FilePathWatcherTest, DestroyWithPendingNotification) {
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  FilePathWatcher watcher;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(WriteFile(test_file(), "content"));
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_MultipleWatchersSingleFile DISABLED_MultipleWatchersSingleFile
#else
#define MAYBE_MultipleWatchersSingleFile MultipleWatchersSingleFile
#endif
TEST_F(FilePathWatcherTest, MAYBE_MultipleWatchersSingleFile) {
  FilePathWatcher watcher1, watcher2;
  std::unique_ptr<TestDelegate> delegate1(new TestDelegate(collector()));
  std::unique_ptr<TestDelegate> delegate2(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher1, delegate1.get(),
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher2, delegate2.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_NonExistentDirectory DISABLED_NonExistentDirectory
#else
#define MAYBE_NonExistentDirectory NonExistentDirectory
#endif
TEST_F(FilePathWatcherTest, MAYBE_NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::CreateDirectory(dir));

  ASSERT_TRUE(WriteFile(file, "content"));

  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvent());
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_DirectoryChain DISABLED_DirectoryChain
#else
#define MAYBE_DirectoryChain DirectoryChain
#endif
TEST_F(FilePathWatcherTest, MAYBE_DirectoryChain) {
  FilePath path(temp_dir_.GetPath());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(base::StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }

  FilePathWatcher watcher;
  FilePath file(path.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  FilePath sub_path(temp_dir_.GetPath());
  for (std::vector<std::string>::const_iterator d(dir_names.begin());
       d != dir_names.end(); ++d) {
    sub_path = sub_path.AppendASCII(*d);
    ASSERT_TRUE(base::CreateDirectory(sub_path));
  }
  VLOG(1) << "Create File";
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_DisappearingDirectory DISABLED_DisappearingDirectory
#else
#define MAYBE_DisappearingDirectory DisappearingDirectory
#endif
TEST_F(FilePathWatcherTest, MAYBE_DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::DeletePathRecursively(dir));
  ASSERT_TRUE(WaitForEvent());
}

// Tests that a file that is deleted and reappears is tracked correctly.
#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_DeleteAndRecreate DISABLED_DeleteAndRecreate
#else
#define MAYBE_DeleteAndRecreate DeleteAndRecreate
#endif
TEST_F(FilePathWatcherTest, MAYBE_DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::DeleteFile(test_file()));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_WatchDirectory DISABLED_WatchDirectory
#else
#define MAYBE_WatchDirectory WatchDirectory
#endif
TEST_F(FilePathWatcherTest, MAYBE_WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dir, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation";
  ASSERT_TRUE(WaitForEvent());

#if !BUILDFLAG(IS_APPLE)
  // Mac implementation does not detect files modified in a directory.
  ASSERT_TRUE(WriteFile(file1, "content v2"));
  VLOG(1) << "Waiting for file1 modification";
  ASSERT_TRUE(WaitForEvent());
#endif  // !BUILDFLAG(IS_APPLE)

  ASSERT_TRUE(base::DeleteFile(file1));
  VLOG(1) << "Waiting for file1 deletion";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation";
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_MoveParent DISABLED_MoveParent
#else
#define MAYBE_MoveParent MoveParent
#endif
TEST_F(FilePathWatcherTest, MAYBE_MoveParent) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath dest(temp_dir_.GetPath().AppendASCII("dest"));
  FilePath subdir(dir.AppendASCII("subdir"));
  FilePath file(subdir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &file_watcher, file_delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));
  std::unique_ptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(subdir, &subdir_watcher, subdir_delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Setup a directory hierarchy.
  ASSERT_TRUE(base::CreateDirectory(subdir));
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvent());

  // Move the parent directory.
  base::Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_RecursiveWatch DISABLED_RecursiveWatch
#else
#define MAYBE_RecursiveWatch RecursiveWatch
#endif
TEST_F(FilePathWatcherTest, MAYBE_RecursiveWatch) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  bool setup_result = SetupWatch(dir, &watcher, delegate.get(),
                                 FilePathWatcher::Type::kRecursive);
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    ASSERT_FALSE(setup_result);
    return;
  }
  ASSERT_TRUE(setup_result);

  // Main directory("dir") creation.
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(WaitForEvent());

  // Create "$dir/file1".
  FilePath file1(dir.AppendASCII("file1"));
  ASSERT_TRUE(WriteFile(file1, "content"));
  ASSERT_TRUE(WaitForEvent());

  // Create "$dir/subdir".
  FilePath subdir(dir.AppendASCII("subdir"));
  ASSERT_TRUE(base::CreateDirectory(subdir));
  ASSERT_TRUE(WaitForEvent());

  // Create "$dir/subdir/subdir2".
  FilePath subdir2(subdir.AppendASCII("subdir2"));
  ASSERT_TRUE(base::CreateDirectory(subdir2));
  ASSERT_TRUE(WaitForEvent());

  // Rename "$dir/subdir/subdir2" to "$dir/subdir/subdir2b".
  FilePath subdir2b(subdir.AppendASCII("subdir2b"));
  base::Move(subdir2, subdir2b);
  ASSERT_TRUE(WaitForEvent());

// Mac and Win don't generate events for Touch.
// Android TouchFile returns false.
#if !(BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))
  // Touch "$dir".
  Time access_time;
  ASSERT_TRUE(Time::FromString("Wed, 16 Nov 1994, 00:00:00", &access_time));
  ASSERT_TRUE(base::TouchFile(dir, access_time, access_time));
  ASSERT_TRUE(WaitForEvent());
#endif  // !(BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))

  // Create "$dir/subdir/subdir_file1".
  FilePath subdir_file1(subdir.AppendASCII("subdir_file1"));
  ASSERT_TRUE(WriteFile(subdir_file1, "content"));
  ASSERT_TRUE(WaitForEvent());

  // Create "$dir/subdir/subdir_child_dir".
  FilePath subdir_child_dir(subdir.AppendASCII("subdir_child_dir"));
  ASSERT_TRUE(base::CreateDirectory(subdir_child_dir));
  ASSERT_TRUE(WaitForEvent());

  // Create "$dir/subdir/subdir_child_dir/child_dir_file1".
  FilePath child_dir_file1(subdir_child_dir.AppendASCII("child_dir_file1"));
  ASSERT_TRUE(WriteFile(child_dir_file1, "content v2"));
  ASSERT_TRUE(WaitForEvent());

  // Write into "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(WriteFile(child_dir_file1, "content"));
  ASSERT_TRUE(WaitForEvent());

// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#if !BUILDFLAG(IS_ANDROID)
  // Modify "$dir/subdir/subdir_child_dir/child_dir_file1" attributes.
  ASSERT_TRUE(base::MakeFileUnreadable(child_dir_file1));
  ASSERT_TRUE(WaitForEvent());
#endif

  // Delete "$dir/subdir/subdir_file1".
  ASSERT_TRUE(base::DeleteFile(subdir_file1));
  ASSERT_TRUE(WaitForEvent());

  // Delete "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(base::DeleteFile(child_dir_file1));
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
// Apps cannot create symlinks on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
//
// This test is disabled on Fuchsia since it doesn't support symlinking.
TEST_F(FilePathWatcherTest, RecursiveWithSymLink) {
  if (!FilePathWatcher::RecursiveWatchAvailable())
    return;

  FilePathWatcher watcher;
  FilePath test_dir(temp_dir_.GetPath().AppendASCII("test_dir"));
  ASSERT_TRUE(base::CreateDirectory(test_dir));
  FilePath symlink(test_dir.AppendASCII("symlink"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(symlink, &watcher, delegate.get(),
                         FilePathWatcher::Type::kRecursive));

  // Link creation.
  FilePath target1(temp_dir_.GetPath().AppendASCII("target1"));
  ASSERT_TRUE(base::CreateSymbolicLink(target1, symlink));
  ASSERT_TRUE(WaitForEvent());

  // Target1 creation.
  ASSERT_TRUE(base::CreateDirectory(target1));
  ASSERT_TRUE(WaitForEvent());

  // Create a file in target1.
  FilePath target1_file(target1.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target1_file, "content"));
  ASSERT_TRUE(WaitForEvent());

  // Link change.
  FilePath target2(temp_dir_.GetPath().AppendASCII("target2"));
  ASSERT_TRUE(base::CreateDirectory(target2));
  ASSERT_TRUE(base::DeleteFile(symlink));
  ASSERT_TRUE(base::CreateSymbolicLink(target2, symlink));
  ASSERT_TRUE(WaitForEvent());

  // Create a file in target2.
  FilePath target2_file(target2.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target2_file, "content"));
  ASSERT_TRUE(WaitForEvent());
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.
#define MAYBE_MoveChild DISABLED_MoveChild
#else
#define MAYBE_MoveChild MoveChild
#endif
TEST_F(FilePathWatcherTest, MAYBE_MoveChild) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath source_dir(temp_dir_.GetPath().AppendASCII("source"));
  FilePath source_subdir(source_dir.AppendASCII("subdir"));
  FilePath source_file(source_subdir.AppendASCII("file"));
  FilePath dest_dir(temp_dir_.GetPath().AppendASCII("dest"));
  FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  FilePath dest_file(dest_subdir.AppendASCII("file"));

  // Setup a directory hierarchy.
  ASSERT_TRUE(base::CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  std::unique_ptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dest_file, &file_watcher, file_delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));
  std::unique_ptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dest_subdir, &subdir_watcher, subdir_delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(base::Move(source_dir, dest_dir));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that changing attributes on a file is caught
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/851641): Re-enable for Fuchsia when inotify is fixed.

// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#define MAYBE_FileAttributesChanged DISABLED_FileAttributesChanged
#else
#define MAYBE_FileAttributesChanged FileAttributesChanged
#endif  // BUILDFLAG(IS_ANDROID)
TEST_F(FilePathWatcherTest, MAYBE_FileAttributesChanged) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(base::MakeFileUnreadable(test_file()));
  ASSERT_TRUE(WaitForEvent());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Verify that creating a symlink is caught.
TEST_F(FilePathWatcherTest, CreateLink) {
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is created.
  // Note that test_file() doesn't have to exist.
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that deleting a symlink is caught.
TEST_F(FilePathWatcherTest, DeleteLink) {
  // Unfortunately this test case only works if the link target exists.
  // TODO(craig) fix this as part of crbug.com/91561.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is deleted.
  ASSERT_TRUE(base::DeleteFile(test_link()));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that modifying a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, ModifiedLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that creating a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, CreateTargetLinkedFile) {
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is created.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that deleting a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, DeleteTargetLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is deleted.
  ASSERT_TRUE(base::DeleteFile(test_file()));
  ASSERT_TRUE(WaitForEvent());
}

// Verify that watching a file whose parent directory is a link that
// doesn't exist yet works if the symlink is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart1) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // dir/file should exist.
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  // Note that we are watching dir.lnk/file which doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  VLOG(1) << "Waiting for link creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvent());
}

// Verify that watching a file whose parent directory is a
// dangling symlink works if the directory is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart2) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Now create the link from dir.lnk pointing to dir but
  // neither dir nor dir/file exist yet.
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for dir/file creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvent());
}

// Verify that watching a file with a symlink on the path
// to the file works.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart3) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file but the file doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvent());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvent());
}

// Regression tests that FilePathWatcherImpl does not leave its reference in
// `g_inotify_reader` due to a race in recursive watch.
// See https://crbug.com/990004.
TEST_F(FilePathWatcherTest, RacyRecursiveWatch) {
  if (!FilePathWatcher::RecursiveWatchAvailable())
    GTEST_SKIP();

  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));

  // Create and delete many subdirs. 20 is an arbitrary number big enough
  // to have more chances to make FilePathWatcherImpl leak watchers.
  std::vector<FilePath> subdirs;
  for (int i = 0; i < 20; ++i)
    subdirs.emplace_back(dir.AppendASCII(base::StringPrintf("subdir_%d", i)));

  Thread subdir_updater("SubDir Updater");
  ASSERT_TRUE(subdir_updater.Start());

  auto subdir_update_task = base::BindLambdaForTesting([&]() {
    for (const auto& subdir : subdirs) {
      // First update event to trigger watch callback.
      ASSERT_TRUE(CreateDirectory(subdir));

      // Second update event. The notification sent for this event will race
      // with the upcoming deletion of the directory below. This test is about
      // verifying that the impl handles this.
      FilePath subdir_file(subdir.AppendASCII("subdir_file"));
      ASSERT_TRUE(WriteFile(subdir_file, "content"));

      // Racy subdir delete to trigger watcher leak.
      ASSERT_TRUE(DeletePathRecursively(subdir));
    }
  });

  // Try the racy subdir update 100 times.
  for (int i = 0; i < 100; ++i) {
    RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    // Keep watch callback in `watcher_callback` so that "watcher.reset()"
    // inside does not release the callback and the lambda capture with it.
    // Otherwise, accessing `run_loop` as part of the lamda capture would be
    // use-after-free under asan.
    auto watcher_callback =
        base::BindLambdaForTesting([&](const FilePath& path, bool error) {
          // Release watchers in callback so that the leaked watchers of
          // the subdir stays. Otherwise, when the subdir is deleted,
          // its delete event would clean up leaked watchers in
          // `g_inotify_reader`.
          watcher.reset();

          run_loop.Quit();
        });

    bool setup_result = watcher->Watch(dir, FilePathWatcher::Type::kRecursive,
                                       watcher_callback);
    ASSERT_TRUE(setup_result);

    subdir_updater.task_runner()->PostTask(FROM_HERE, subdir_update_task);

    // Wait for the watch callback.
    run_loop.Run();

    // `watcher` should have been released.
    ASSERT_FALSE(watcher);

    // There should be no outstanding watchers.
    ASSERT_FALSE(FilePathWatcher::HasWatchesForTest());
  }
}

// Verify that "Watch()" returns false and callback is not invoked when limit is
// hit during setup.
TEST_F(FilePathWatcherTest, InotifyLimitInWatch) {
  auto watcher = std::make_unique<FilePathWatcher>();

  // "test_file()" is like "/tmp/__unique_path__/FilePathWatcherTest" and has 4
  // dir components ("/" + 3 named parts). "Watch()" creates inotify watches
  // for each dir component of the given dir. It would fail with limit set to 1.
  ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(1);
  ASSERT_FALSE(watcher->Watch(
      test_file(), FilePathWatcher::Type::kNonRecursive,
      base::BindLambdaForTesting(
          [&](const FilePath& path, bool error) { ADD_FAILURE(); })));

  // Triggers update but callback should not be invoked.
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  // Ensures that the callback did not happen.
  base::RunLoop().RunUntilIdle();
}

// Verify that "error=true" callback happens when limit is hit during update.
TEST_F(FilePathWatcherTest, InotifyLimitInUpdate) {
  enum kTestType {
    // Destroy watcher in "error=true" callback.
    // No crash/deadlock when releasing watcher in the callback.
    kDestroyWatcher,

    // Do not destroy watcher in "error=true" callback.
    kDoNothing,
  };

  for (auto callback_type : {kDestroyWatcher, kDoNothing}) {
    SCOPED_TRACE(testing::Message() << "type=" << callback_type);

    base::RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        base::BindLambdaForTesting([&](const FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error)
            return;

          error_callback_called = true;

          if (callback_type == kDestroyWatcher)
            watcher.reset();

          run_loop.Quit();
        });
    ASSERT_TRUE(watcher->Watch(
        test_file(), FilePathWatcher::Type::kNonRecursive, watcher_callback));

    ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(1);

    // Triggers update and over limit.
    ASSERT_TRUE(WriteFile(test_file(), "content"));

    run_loop.Run();

    // More update but no more callback should happen.
    ASSERT_TRUE(DeleteFile(test_file()));
    base::RunLoop().RunUntilIdle();
  }
}

// Similar to InotifyLimitInUpdate but test a recursive watcher.
TEST_F(FilePathWatcherTest, InotifyLimitInUpdateRecursive) {
  enum kTestType {
    // Destroy watcher in "error=true" callback.
    // No crash/deadlock when releasing watcher in the callback.
    kDestroyWatcher,

    // Do not destroy watcher in "error=true" callback.
    kDoNothing,
  };

  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));

  for (auto callback_type : {kDestroyWatcher, kDoNothing}) {
    SCOPED_TRACE(testing::Message() << "type=" << callback_type);

    base::RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        base::BindLambdaForTesting([&](const FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error)
            return;

          error_callback_called = true;

          if (callback_type == kDestroyWatcher)
            watcher.reset();

          run_loop.Quit();
        });
    ASSERT_TRUE(watcher->Watch(dir, FilePathWatcher::Type::kRecursive,
                               watcher_callback));

    constexpr size_t kMaxLimit = 10u;
    ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(
        kMaxLimit);

    // Triggers updates and over limit.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      base::FilePath subdir =
          dir.AppendASCII(base::StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(CreateDirectory(subdir));
    }

    run_loop.Run();

    // More update but no more callback should happen.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      base::FilePath subdir =
          dir.AppendASCII(base::StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(DeleteFile(subdir));
    }
    base::RunLoop().RunUntilIdle();
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// TODO(fxbug.dev/60109): enable BUILDFLAG(IS_FUCHSIA) when implemented.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

TEST_F(FilePathWatcherTest, ReturnFullPath_RecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, delegate.get(),
                            {.type = base::FilePathWatcher::Type::kRecursive,
                             .report_modified_path = true}));

  // Triggers three events:
  // create on /watched_folder/file
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths{file, file, file, file};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_RecursiveInNestedFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath subfolder(watched_folder.AppendASCII("subfolder"));
  FilePath file(subfolder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, delegate.get(),
                            {.type = base::FilePathWatcher::Type::kRecursive,
                             .report_modified_path = true}));

  // Expects create on /watched_folder/subfolder.
  ASSERT_TRUE(CreateDirectory(subfolder));
  ASSERT_TRUE(WaitForEvent());

  // Triggers two events:
  // create on /watched_folder/subfolder/file.
  // modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  // Expects modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder/subfolder/file.
  ASSERT_TRUE(DeleteFile(file));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder/subfolder.
  ASSERT_TRUE(DeleteFile(subfolder));
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths{subfolder, file, file,
                                             file,      file, subfolder};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(base::CreateDirectory(watched_folder));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, delegate.get(),
                            {.type = base::FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers three events:
  // create on /watched_folder/file.
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths{file, file, file, file};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveRemoveEnclosingFolder) {
  FilePathWatcher directory_watcher;
  FilePath root_folder(temp_dir_.GetPath().AppendASCII("root_folder"));
  FilePath folder(root_folder.AppendASCII("folder"));
  FilePath watched_folder(folder.AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(base::CreateDirectory(watched_folder));
  ASSERT_TRUE(WriteFile(file, "test"));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, delegate.get(),
                            {.type = base::FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers four events:
  // delete on /watched_folder/file.
  // delete on /watched_folder twice.
  ASSERT_TRUE(DeletePathRecursively(folder));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths{file, watched_folder,
                                             watched_folder};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_RecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(base::CreateDirectory(watched_folder));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, delegate.get(),
                            {.type = base::FilePathWatcher::Type::kRecursive}));

  // Triggers three events:
  // create on /watched_folder.
  // modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths{watched_folder, watched_folder,
                                             watched_folder, watched_folder};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(base::CreateDirectory(watched_folder));

  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(SetupWatchWithOptions(
      watched_folder, &directory_watcher, delegate.get(),
      {.type = base::FilePathWatcher::Type::kNonRecursive}));

  // Triggers three events:
  // Expects create /watched_folder.
  // Expects modify /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(WaitForEvent());

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  ASSERT_TRUE(WaitForEvent());

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  ASSERT_TRUE(WaitForEvent());

  std::vector<base::FilePath> expected_paths {watched_folder, watched_folder,
                              watched_folder, watched_folder};
  EXPECT_EQ(delegate->get_observed_paths(), expected_paths);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

enum Permission {
  Read,
  Write,
  Execute
};

#if BUILDFLAG(IS_APPLE)
bool ChangeFilePermissions(const FilePath& path, Permission perm, bool allow) {
  struct stat stat_buf;

  if (stat(path.value().c_str(), &stat_buf) != 0)
    return false;

  mode_t mode = 0;
  switch (perm) {
    case Read:
      mode = S_IRUSR | S_IRGRP | S_IROTH;
      break;
    case Write:
      mode = S_IWUSR | S_IWGRP | S_IWOTH;
      break;
    case Execute:
      mode = S_IXUSR | S_IXGRP | S_IXOTH;
      break;
    default:
      ADD_FAILURE() << "unknown perm " << perm;
      return false;
  }
  if (allow) {
    stat_buf.st_mode |= mode;
  } else {
    stat_buf.st_mode &= ~mode;
  }
  return chmod(path.value().c_str(), stat_buf.st_mode) == 0;
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Linux implementation of FilePathWatcher doesn't catch attribute changes.
// http://crbug.com/78043
// Windows implementation of FilePathWatcher catches attribute changes that
// don't affect the path being watched.
// http://crbug.com/78045

// Verify that changing attributes on a directory works.
TEST_F(FilePathWatcherTest, DirAttributesChanged) {
  FilePath test_dir1(
      temp_dir_.GetPath().AppendASCII("DirAttributesChangedDir1"));
  FilePath test_dir2(test_dir1.AppendASCII("DirAttributesChangedDir2"));
  FilePath test_file(test_dir2.AppendASCII("DirAttributesChangedFile"));
  // Setup a directory hierarchy.
  ASSERT_TRUE(base::CreateDirectory(test_dir1));
  ASSERT_TRUE(base::CreateDirectory(test_dir2));
  ASSERT_TRUE(WriteFile(test_file, "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // We should not get notified in this case as it hasn't affected our ability
  // to access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, false));
  ASSERT_FALSE(WaitForEventWithTimeout(TestTimeouts::tiny_timeout()));
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, true));

  // We should get notified in this case because filepathwatcher can no
  // longer access the file
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, false));
  ASSERT_TRUE(WaitForEvent());
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, true));
}

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_MAC)

// Fail fast if trying to trivially watch a non-existent item.
TEST_F(FilePathWatcherTest, TrivialNoDir) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath non_existent = tmp_dir.Append(FILE_PATH_LITERAL("nope"));

  FilePathWatcher watcher;
  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_FALSE(SetupWatch(non_existent, &watcher, delegate.get(),
                          FilePathWatcher::Type::kTrivial));
}

// Succeed starting a watch on a directory.
TEST_F(FilePathWatcherTest, TrivialDirStart) {
  const FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, delegate.get(),
                         FilePathWatcher::Type::kTrivial));
}

// Observe a change on a directory
TEST_F(FilePathWatcherTest, TrivialDirChange) {
  const FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, delegate.get(),
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(TouchFile(tmp_dir, base::Time::Now(), base::Time::Now()));
  ASSERT_TRUE(WaitForEvent());
}

// Observe no change when a parent is modified.
TEST_F(FilePathWatcherTest, TrivialParentDirChange) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath sub_dir1 = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));
  const FilePath sub_dir2 = sub_dir1.Append(FILE_PATH_LITERAL("subdir_redux"));

  ASSERT_TRUE(CreateDirectory(sub_dir1));
  ASSERT_TRUE(CreateDirectory(sub_dir2));

  FilePathWatcher watcher;
  auto delegate = std::make_unique<TestDelegate>(collector());
  ASSERT_TRUE(SetupWatch(sub_dir2, &watcher, delegate.get(),
                         FilePathWatcher::Type::kTrivial));

  // There should be no notification for a change to |sub_dir2|'s parent.
  ASSERT_TRUE(Move(sub_dir1, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  ASSERT_FALSE(WaitForEventWithTimeout(TestTimeouts::tiny_timeout()));
}

// Do not crash when a directory is moved; https://crbug.com/1156603.
TEST_F(FilePathWatcherTest, TrivialDirMove) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath sub_dir = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));

  ASSERT_TRUE(CreateDirectory(sub_dir));

  FilePathWatcher watcher;
  auto delegate = std::make_unique<TestDelegate>(collector());
  delegate->set_expect_error();
  ASSERT_TRUE(SetupWatch(sub_dir, &watcher, delegate.get(),
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(Move(sub_dir, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  ASSERT_TRUE(WaitForEvent());
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace

}  // namespace base
