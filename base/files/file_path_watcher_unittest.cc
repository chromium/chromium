// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#include <aclapi.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#endif

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif  // defined(OS_POSIX)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/files/file_path_watcher_linux.h"
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
  NotificationCollector() : task_runner_(ThreadTaskRunnerHandle::Get()) {}

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

  // TestDelegateBase:
  void OnFileChanged(const FilePath& path, bool error) override {
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
};

class FilePathWatcherTest : public testing::Test {
 public:
  FilePathWatcherTest()
#if defined(OS_POSIX)
      : task_environment_(test::TaskEnvironment::MainThreadType::IO)
#endif
  {
  }

  FilePathWatcherTest(const FilePathWatcherTest&) = delete;
  FilePathWatcherTest& operator=(const FilePathWatcherTest&) = delete;
  ~FilePathWatcherTest() override = default;

 protected:
  void SetUp() override {
#if defined(OS_ANDROID)
    // Watching files is only permitted when all parent directories are
    // accessible, which is not the case for the default temp directory
    // on Android which is under /data/data.  Use /sdcard instead.
    // TODO(pauljensen): Remove this when crbug.com/475568 is fixed.
    FilePath parent_dir;
    ASSERT_TRUE(android::GetExternalStorageDirectory(&parent_dir));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(parent_dir));
#else   // defined(OS_ANDROID)
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#endif  // defined(OS_ANDROID)
    collector_ = new NotificationCollector();
  }

  void TearDown() override { RunLoop().RunUntilIdle(); }

  FilePath test_file() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest");
  }

  FilePath test_link() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest.lnk");
  }

  bool SetupWatch(const FilePath& target,
                  FilePathWatcher* watcher,
                  TestDelegateBase* delegate,
                  FilePathWatcher::Type watch_type) WARN_UNUSED_RESULT;

  bool WaitForEvents() WARN_UNUSED_RESULT {
    return WaitForEventsWithTimeout(TestTimeouts::action_timeout());
  }

  bool WaitForEventsWithTimeout(TimeDelta timeout) WARN_UNUSED_RESULT {
    RunLoop run_loop;
    collector_->Reset(run_loop.QuitClosure());

    // Make sure we timeout if we don't get notified.
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, NewFile) {
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
}

// Verify that moving the file into place is caught.
TEST_F(FilePathWatcherTest, MovedFile) {
  FilePath source_file(temp_dir_.GetPath().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(base::Move(source_file, test_file()));
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is deleted.
  base::DeleteFile(test_file());
  ASSERT_TRUE(WaitForEvents());
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

TEST_F(FilePathWatcherTest, MultipleWatchersSingleFile) {
  FilePathWatcher watcher1, watcher2;
  std::unique_ptr<TestDelegate> delegate1(new TestDelegate(collector()));
  std::unique_ptr<TestDelegate> delegate2(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher1, delegate1.get(),
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher2, delegate2.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::CreateDirectory(dir));

  ASSERT_TRUE(WriteFile(file, "content"));

  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
TEST_F(FilePathWatcherTest, DirectoryChain) {
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
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::DeletePathRecursively(dir));
  ASSERT_TRUE(WaitForEvents());
}

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::DeleteFile(test_file()));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dir, &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(base::CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation";
  ASSERT_TRUE(WaitForEvents());

#if !defined(OS_APPLE)
  // Mac implementation does not detect files modified in a directory.
  ASSERT_TRUE(WriteFile(file1, "content v2"));
  VLOG(1) << "Waiting for file1 modification";
  ASSERT_TRUE(WaitForEvents());
#endif  // !OS_APPLE

  ASSERT_TRUE(base::DeleteFile(file1));
  VLOG(1) << "Waiting for file1 deletion";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, MoveParent) {
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
  ASSERT_TRUE(WaitForEvents());

  // Move the parent directory.
  base::Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, RecursiveWatch) {
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
  ASSERT_TRUE(WaitForEvents());

  // Create "$dir/file1".
  FilePath file1(dir.AppendASCII("file1"));
  ASSERT_TRUE(WriteFile(file1, "content"));
  ASSERT_TRUE(WaitForEvents());

  // Create "$dir/subdir".
  FilePath subdir(dir.AppendASCII("subdir"));
  ASSERT_TRUE(base::CreateDirectory(subdir));
  ASSERT_TRUE(WaitForEvents());

// Mac and Win don't generate events for Touch.
// Android TouchFile returns false.
#if !(defined(OS_APPLE) || defined(OS_WIN) || defined(OS_ANDROID))
  // Touch "$dir".
  Time access_time;
  ASSERT_TRUE(Time::FromString("Wed, 16 Nov 1994, 00:00:00", &access_time));
  ASSERT_TRUE(base::TouchFile(dir, access_time, access_time));
  ASSERT_TRUE(WaitForEvents());
#endif  // !(defined(OS_APPLE) || defined(OS_WIN) || defined(OS_ANDROID))

  // Create "$dir/subdir/subdir_file1".
  FilePath subdir_file1(subdir.AppendASCII("subdir_file1"));
  ASSERT_TRUE(WriteFile(subdir_file1, "content"));
  ASSERT_TRUE(WaitForEvents());

  // Create "$dir/subdir/subdir_child_dir".
  FilePath subdir_child_dir(subdir.AppendASCII("subdir_child_dir"));
  ASSERT_TRUE(base::CreateDirectory(subdir_child_dir));
  ASSERT_TRUE(WaitForEvents());

  // Create "$dir/subdir/subdir_child_dir/child_dir_file1".
  FilePath child_dir_file1(subdir_child_dir.AppendASCII("child_dir_file1"));
  ASSERT_TRUE(WriteFile(child_dir_file1, "content v2"));
  ASSERT_TRUE(WaitForEvents());

  // Write into "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(WriteFile(child_dir_file1, "content"));
  ASSERT_TRUE(WaitForEvents());

// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#if !defined(OS_ANDROID)
  // Modify "$dir/subdir/subdir_child_dir/child_dir_file1" attributes.
  ASSERT_TRUE(base::MakeFileUnreadable(child_dir_file1));
  ASSERT_TRUE(WaitForEvents());
#endif

  // Delete "$dir/subdir/subdir_file1".
  ASSERT_TRUE(base::DeleteFile(subdir_file1));
  ASSERT_TRUE(WaitForEvents());

  // Delete "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(base::DeleteFile(child_dir_file1));
  ASSERT_TRUE(WaitForEvents());
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
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
  ASSERT_TRUE(WaitForEvents());

  // Target1 creation.
  ASSERT_TRUE(base::CreateDirectory(target1));
  ASSERT_TRUE(WaitForEvents());

  // Create a file in target1.
  FilePath target1_file(target1.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target1_file, "content"));
  ASSERT_TRUE(WaitForEvents());

  // Link change.
  FilePath target2(temp_dir_.GetPath().AppendASCII("target2"));
  ASSERT_TRUE(base::CreateDirectory(target2));
  ASSERT_TRUE(base::DeleteFile(symlink));
  ASSERT_TRUE(base::CreateSymbolicLink(target2, symlink));
  ASSERT_TRUE(WaitForEvents());

  // Create a file in target2.
  FilePath target2_file(target2.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target2_file, "content"));
  ASSERT_TRUE(WaitForEvents());
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)

TEST_F(FilePathWatcherTest, MoveChild) {
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
  ASSERT_TRUE(WaitForEvents());
}

// Verify that changing attributes on a file is caught
#if defined(OS_ANDROID)
// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#define FileAttributesChanged DISABLED_FileAttributesChanged
#endif  // defined(OS_ANDROID
TEST_F(FilePathWatcherTest, FileAttributesChanged) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  std::unique_ptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get(),
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(base::MakeFileUnreadable(test_file()));
  ASSERT_TRUE(WaitForEvents());
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)

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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(base::DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
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

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

enum Permission {
  Read,
  Write,
  Execute
};

#if defined(OS_APPLE)
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
#endif  // defined(OS_APPLE)

#if defined(OS_APPLE)
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
  ASSERT_FALSE(WaitForEventsWithTimeout(TestTimeouts::tiny_timeout()));
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, true));

  // We should get notified in this case because filepathwatcher can no
  // longer access the file
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, false));
  ASSERT_TRUE(WaitForEvents());
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, true));
}

#endif  // OS_APPLE

#if defined(OS_MAC)

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
  ASSERT_TRUE(WaitForEvents());
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
  ASSERT_FALSE(WaitForEvents());
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
  ASSERT_TRUE(WaitForEvents());
}

#endif  // defined(OS_MAC)

}  // namespace

}  // namespace base
