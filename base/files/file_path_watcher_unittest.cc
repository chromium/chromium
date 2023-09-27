// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_path_watcher_inotify.h"
#include "base/format_macros.h"
#endif

namespace base {

namespace {

AtomicSequenceNumber g_next_delegate_id;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// inotify fires two events - one for each file creation + modification.
constexpr size_t kExpectedEventsForNewFileWrite = 2;
#else
constexpr size_t kExpectedEventsForNewFileWrite = 1;
#endif

enum class ExpectedEventsSinceLastWait { kNone, kSome };

struct Event {
  FilePath path;
  bool error;
  // TODO(https://crbug.com/1425601): Support change types.

  bool operator==(const Event& other) const {
    return error == other.error && path == other.path;
  }
};

std::ostream& operator<<(std::ostream& os, const Event& event) {
  return os << "Event{ error: " << (event.error ? "true" : "false")
            << ", path: " << event.path << " }";
}

using EventListMatcher = testing::Matcher<std::list<Event>>;

void SpinEventLoopForABit() {
  base::RunLoop loop;
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), TestTimeouts::tiny_timeout());
  loop.Run();
}

// Returns the reason why `value` matches, or doesn't match, `matcher`.
template <typename MatcherType, typename Value>
std::string Explain(const MatcherType& matcher, const Value& value) {
  testing::StringMatchResultListener listener;
  testing::ExplainMatchResult(matcher, value, &listener);
  return listener.str();
}

// Enables an accumulative, add-as-you-go pattern for expecting events:
//   - Do something that should fire `event1` on `delegate`
//   - Add `event1` to an `accumulated_event_expecter`
//   - Wait until `delegate` matches { `event1` }
//   - Do something that should fire `event2` on `delegate`
//   - Add `event2` to an `accumulated_event_expecter`
//   - Wait until `delegate` matches { `event1`, `event2` }
//   - ...
//
// These tests use an accumulative pattern due to the potential for
// false-positives, given that all we know is the number of changes at a given
// path (which is often fixed) and whether or not an error occurred (which is
// rare).
//
// TODO(https://crbug.com/1425601): This is not a common pattern. Generally,
// expectations are specified all-in-one at the start of a test, like so:
//   - Expect events { `event1`, `event2` }
//   - Do something that should fire `event1` on `delegate`
//   - Do something that should fire `event2` on `delegate`
//   - Wait until `delegate` matches { `event1`, `event2` }
//
// The potential for false-positives is much less if event types are known. We
// should consider moving towards the latter pattern once that is supported.
class AccumulatingEventExpecter {
 public:
  EventListMatcher GetMatcher() {
    return testing::ContainerEq(expected_events_);
  }

  ExpectedEventsSinceLastWait GetAndResetExpectedEventsSinceLastWait() {
    auto temp = expected_events_since_last_wait_;
    expected_events_since_last_wait_ = ExpectedEventsSinceLastWait::kNone;
    return temp;
  }

  // TODO(https://crbug.com/1425601): Add a version of this method which
  // includes the type of change.
  void AddExpectedEventForPath(const FilePath& path, bool error = false) {
    expected_events_.emplace_back(path, error);
    expected_events_since_last_wait_ = ExpectedEventsSinceLastWait::kSome;
  }

 private:
  std::list<Event> expected_events_;
  ExpectedEventsSinceLastWait expected_events_since_last_wait_ =
      ExpectedEventsSinceLastWait::kNone;
};

class TestDelegateBase : public SupportsWeakPtr<TestDelegateBase> {
 public:
  TestDelegateBase() = default;
  TestDelegateBase(const TestDelegateBase&) = delete;
  TestDelegateBase& operator=(const TestDelegateBase&) = delete;
  virtual ~TestDelegateBase() = default;

  virtual void OnFileChanged(const FilePath& path, bool error) = 0;
};

// Receives and accumulates notifications from a specific `FilePathWatcher`.
// This class is not thread safe. All methods must be called from the sequence
// the instance is constructed on.
class TestDelegate : public TestDelegateBase {
 public:
  TestDelegate() : id_(g_next_delegate_id.GetNext()) {}
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  // TestDelegateBase:
  void OnFileChanged(const FilePath& path, bool error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    received_events_.emplace_back(path, error);
  }

  // Gives all in-flight events a chance to arrive, then forgets all events that
  // have been received by this delegate. This method may be a useful reset
  // after performing a file system operation that may result in a variable
  // sequence of events.
  void SpinAndDiscardAllReceivedEvents() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SpinEventLoopForABit();
    received_events_.clear();
  }

  // Spin the event loop until `received_events_` match `matcher`, or we time
  // out.
  void RunUntilEventsMatch(
      const EventListMatcher& matcher,
      ExpectedEventsSinceLastWait expected_events_since_last_wait,
      const Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (expected_events_since_last_wait == ExpectedEventsSinceLastWait::kNone) {
      // Give unexpected events a chance to arrive.
      SpinEventLoopForABit();
    }

    EXPECT_TRUE(test::RunUntil([&]() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return testing::Matches(matcher)(received_events_);
    })) << "Timed out attemping to match events at "
        << location.file_name() << ":" << location.line_number() << std::endl
        << Explain(matcher, received_events_);
  }
  // Convenience method for above.
  void RunUntilEventsMatch(AccumulatingEventExpecter& event_expecter,
                           const Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return RunUntilEventsMatch(
        event_expecter.GetMatcher(),
        event_expecter.GetAndResetExpectedEventsSinceLastWait(), location);
  }

  // Convenience method for when no events are expected.
  void SpinAndExpectNoEvents(const Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return RunUntilEventsMatch(testing::IsEmpty(),
                               ExpectedEventsSinceLastWait::kNone, location);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Uniquely generated ID used to tie events to this delegate.
  const size_t id_;

  std::list<Event> received_events_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

#if BUILDFLAG(IS_FUCHSIA)
// FilePatchWatcherImpl is not implemented (see crbug.com/851641).
// Disable all tests.
#define FilePathWatcherTest DISABLED_FilePathWatcherTest
#endif

class FilePathWatcherTest : public testing::Test {
 public:
  FilePathWatcherTest()
#if BUILDFLAG(IS_POSIX)
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
                  FilePathWatcher::Type watch_type);

  bool SetupWatchWithOptions(const FilePath& target,
                             FilePathWatcher* watcher,
                             TestDelegateBase* delegate,
                             FilePathWatcher::WatchOptions watch_options);

  test::TaskEnvironment task_environment_;

  ScopedTempDir temp_dir_;
};

bool FilePathWatcherTest::SetupWatch(const FilePath& target,
                                     FilePathWatcher* watcher,
                                     TestDelegateBase* delegate,
                                     FilePathWatcher::Type watch_type) {
  return watcher->Watch(
      target, watch_type,
      BindPostTaskToCurrentDefault(BindRepeating(
          &TestDelegateBase::OnFileChanged, delegate->AsWeakPtr())));
}

bool FilePathWatcherTest::SetupWatchWithOptions(
    const FilePath& target,
    FilePathWatcher* watcher,
    TestDelegateBase* delegate,
    FilePathWatcher::WatchOptions watch_options) {
  return watcher->WatchWithOptions(
      target, watch_options,
      BindPostTaskToCurrentDefault(BindRepeating(
          &TestDelegateBase::OnFileChanged, delegate->AsWeakPtr())));
}

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, NewFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_file());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// Basic test: Create the directory and verify that we notice.
TEST_F(FilePathWatcherTest, NewDirectory) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Basic test: Create the directory and verify that we notice.
TEST_F(FilePathWatcherTest, NewDirectoryRecursiveWatch) {
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    return;
  }

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kRecursive));

  ASSERT_TRUE(CreateDirectory(test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, ModifiedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that creating the parent directory of the watched file is not caught.
TEST_F(FilePathWatcherTest, CreateParentDirectory) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  FilePath child(parent.AppendASCII("child"));

  ASSERT_TRUE(SetupWatch(child, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we do not get notified when the parent is created.
  ASSERT_TRUE(CreateDirectory(parent));
  delegate.SpinAndExpectNoEvents();
}

// Verify that changes to the sibling of the watched file are not caught.
TEST_F(FilePathWatcherTest, CreateSiblingFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we do not get notified if a sibling of the watched file is
  // created or modified.
  ASSERT_TRUE(WriteFile(test_file().AddExtensionASCII(".swap"), "content"));
  ASSERT_TRUE(WriteFile(test_file().AddExtensionASCII(".swap"), "new content"));
  delegate.SpinAndExpectNoEvents();
}

// Verify that changes to the sibling of the parent of the watched file are not
// caught.
TEST_F(FilePathWatcherTest, CreateParentSiblingFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  FilePath parent_sibling(temp_dir_.GetPath().AppendASCII("parent_sibling"));
  FilePath child(parent.AppendASCII("child"));
  ASSERT_TRUE(SetupWatch(child, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Don't notice changes to a sibling directory of `parent` while `parent` does
  // not exist.
  ASSERT_TRUE(CreateDirectory(parent_sibling));
  ASSERT_TRUE(DeletePathRecursively(parent_sibling));

  // Don't notice changes to a sibling file of `parent` while `parent` does
  // not exist.
  ASSERT_TRUE(WriteFile(parent_sibling, "do not notice this"));
  ASSERT_TRUE(DeleteFile(parent_sibling));

  // Don't notice the creation of `parent`.
  ASSERT_TRUE(CreateDirectory(parent));

  // Don't notice changes to a sibling directory of `parent` while `parent`
  // exists.
  ASSERT_TRUE(CreateDirectory(parent_sibling));
  ASSERT_TRUE(DeletePathRecursively(parent_sibling));

  // Don't notice changes to a sibling file of `parent` while `parent` exists.
  ASSERT_TRUE(WriteFile(parent_sibling, "do not notice this"));
  ASSERT_TRUE(DeleteFile(parent_sibling));

  delegate.SpinAndExpectNoEvents();
}

// Verify that moving an unwatched file to a watched path is caught.
TEST_F(FilePathWatcherTest, MovedToFile) {
  FilePath source_file(temp_dir_.GetPath().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is moved.
  ASSERT_TRUE(Move(source_file, test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that moving the watched file to an unwatched path is caught.
TEST_F(FilePathWatcherTest, MovedFromFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(Move(test_file(), temp_dir_.GetPath().AppendASCII("dest")));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is deleted.
  DeleteFile(test_file());
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

namespace {

// Used by the DeleteDuringNotify test below.
// Deletes the FilePathWatcher when it's notified.
class Deleter : public TestDelegateBase {
 public:
  explicit Deleter(OnceClosure done_closure)
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
  OnceClosure done_closure_;
};

}  // namespace

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  RunLoop run_loop;
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
  TestDelegate delegate;
  FilePathWatcher watcher;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(WriteFile(test_file(), "content"));
}

TEST_F(FilePathWatcherTest, MultipleWatchersSingleFile) {
  FilePathWatcher watcher1, watcher2;
  TestDelegate delegate1, delegate2;
  AccumulatingEventExpecter event_expecter1, event_expecter2;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher1, &delegate1,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher2, &delegate2,
                         FilePathWatcher::Type::kNonRecursive));

  // Expect to be notified for writing to a new file for each delegate.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter1.AddExpectedEventForPath(test_file());
    event_expecter2.AddExpectedEventForPath(test_file());
  }
  delegate1.RunUntilEventsMatch(event_expecter1);
  delegate2.RunUntilEventsMatch(event_expecter2);
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // The delegate is only watching the file. Parent directory creation should
  // not trigger an event.
  ASSERT_TRUE(CreateDirectory(dir));
  // TODO(https://crbug.com/1432064): Expect that no events are fired.

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
TEST_F(FilePathWatcherTest, DirectoryChain) {
  FilePath path(temp_dir_.GetPath());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }

  FilePathWatcher watcher;
  FilePath file(path.AppendASCII("file"));
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  FilePath sub_path(temp_dir_.GetPath());
  for (std::vector<std::string>::const_iterator d(dir_names.begin());
       d != dir_names.end(); ++d) {
    sub_path = sub_path.AppendASCII(*d);
    ASSERT_TRUE(CreateDirectory(sub_path));
    // TODO(https://crbug.com/1432064): Expect that no events are fired.
  }

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  VLOG(1) << "Create File";
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation + modification";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(DeletePathRecursively(dir));
  event_expecter.AddExpectedEventForPath(file);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/1432044): Figure out why this may fire two events on
  // inotify. Only the file is being watched, so presumably there should only be
  // one deletion event.
  event_expecter.AddExpectedEventForPath(file);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
  delegate.RunUntilEventsMatch(event_expecter);
}

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(DeleteFile(test_file()));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_file());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// TODO(https://crbug.com/1432064): Split into smaller tests.
TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(dir, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

#if !BUILDFLAG(IS_APPLE)
  ASSERT_TRUE(WriteFile(file1, "content v2"));
  // Mac implementation does not detect files modified in a directory.
  // TODO(https://crbug.com/1432064): Expect that no events are fired on Mac.
  // TODO(https://crbug.com/1019297): Consider using FSEvents to support
  // watching a directory and its immediate children, as Type::kNonRecursive
  // does on other platforms.
  VLOG(1) << "Waiting for file1 modification";
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);
#endif  // !BUILDFLAG(IS_APPLE)

  ASSERT_TRUE(DeleteFile(file1));
  VLOG(1) << "Waiting for file1 deletion";
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, MoveParent) {
  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  AccumulatingEventExpecter file_event_expecter, subdir_event_expecter;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath dest(temp_dir_.GetPath().AppendASCII("dest"));
  FilePath subdir(dir.AppendASCII("subdir"));
  FilePath file(subdir.AppendASCII("file"));
  ASSERT_TRUE(SetupWatch(file, &file_watcher, &file_delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(subdir, &subdir_watcher, &subdir_delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Setup a directory hierarchy.
  // We should only get notified on `subdir_delegate` of its creation.
  ASSERT_TRUE(CreateDirectory(subdir));
  subdir_event_expecter.AddExpectedEventForPath(subdir);
  // TODO(https://crbug.com/1432064): Expect that no events are fired on the
  // file delegate.
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    file_event_expecter.AddExpectedEventForPath(file);
    subdir_event_expecter.AddExpectedEventForPath(subdir);
  }
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);

  Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  file_event_expecter.AddExpectedEventForPath(file);
  subdir_event_expecter.AddExpectedEventForPath(subdir);
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);
}

TEST_F(FilePathWatcherTest, RecursiveWatch) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  bool setup_result =
      SetupWatch(dir, &watcher, &delegate, FilePathWatcher::Type::kRecursive);
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    ASSERT_FALSE(setup_result);
    return;
  }
  ASSERT_TRUE(setup_result);

  // TODO(https://crbug.com/1432064): Create a version of this test which also
  // verifies that the events occur on the correct file path if the watcher is
  // set up to record the target of the event.

  // Main directory("dir") creation.
  ASSERT_TRUE(CreateDirectory(dir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/file1".
  FilePath file1(dir.AppendASCII("file1"));
  ASSERT_TRUE(WriteFile(file1, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir".
  FilePath subdir(dir.AppendASCII("subdir"));
  ASSERT_TRUE(CreateDirectory(subdir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir2".
  FilePath subdir2(subdir.AppendASCII("subdir2"));
  ASSERT_TRUE(CreateDirectory(subdir2));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Rename "$dir/subdir/subdir2" to "$dir/subdir/subdir2b".
  FilePath subdir2b(subdir.AppendASCII("subdir2b"));
  Move(subdir2, subdir2b);
  event_expecter.AddExpectedEventForPath(dir);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // inotify generates separate IN_MOVED_TO and IN_MOVED_FROM events for a
  // rename. Since both the source and destination are within the scope of this
  // watch, both events should be received.
  event_expecter.AddExpectedEventForPath(dir);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
  delegate.RunUntilEventsMatch(event_expecter);

// Mac and Win don't generate events for Touch.
// TODO(https://crbug.com/1432064): Add explicit expectations for Mac and Win.
// Android TouchFile returns false.
#if !(BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))
  // Touch "$dir".
  Time access_time;
  ASSERT_TRUE(Time::FromString("Wed, 16 Nov 1994, 00:00:00", &access_time));
  ASSERT_TRUE(TouchFile(dir, access_time, access_time));
  // TODO(https://crbug.com/1432044): Investigate why we're getting two events
  // here from inotify.
  event_expecter.AddExpectedEventForPath(dir);
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);
  // TODO(https://crbug.com/1432064): Add a test touching `subdir`.
#endif  // !(BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))

  // Create "$dir/subdir/subdir_file1".
  FilePath subdir_file1(subdir.AppendASCII("subdir_file1"));
  ASSERT_TRUE(WriteFile(subdir_file1, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir_child_dir".
  FilePath subdir_child_dir(subdir.AppendASCII("subdir_child_dir"));
  ASSERT_TRUE(CreateDirectory(subdir_child_dir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir_child_dir/child_dir_file1".
  FilePath child_dir_file1(subdir_child_dir.AppendASCII("child_dir_file1"));
  ASSERT_TRUE(WriteFile(child_dir_file1, "content v2"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Write into "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(WriteFile(child_dir_file1, "content"));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#if !BUILDFLAG(IS_ANDROID)
  // Modify "$dir/subdir/subdir_child_dir/child_dir_file1" attributes.
  ASSERT_TRUE(MakeFileUnreadable(child_dir_file1));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);
#endif  // !BUILDFLAG(IS_ANDROID))

  // Delete "$dir/subdir/subdir_file1".
  ASSERT_TRUE(DeleteFile(subdir_file1));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Delete "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(DeleteFile(child_dir_file1));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);
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
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    return;
  }

  FilePathWatcher watcher;
  FilePath test_dir(temp_dir_.GetPath().AppendASCII("test_dir"));
  ASSERT_TRUE(CreateDirectory(test_dir));
  FilePath symlink(test_dir.AppendASCII("symlink"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(symlink, &watcher, &delegate,
                         FilePathWatcher::Type::kRecursive));

  // TODO(https://crbug.com/1432064): Figure out what the intended behavior here
  // is. Many symlink operations don't seem to be supported on Mac, while in
  // other cases Mac fires more events than expected.

  // Link creation.
  FilePath target1(temp_dir_.GetPath().AppendASCII("target1"));
  ASSERT_TRUE(CreateSymbolicLink(target1, symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Target1 creation.
  ASSERT_TRUE(CreateDirectory(target1));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create a file in target1.
  FilePath target1_file(target1.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target1_file, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(symlink);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Link change.
  FilePath target2(temp_dir_.GetPath().AppendASCII("target2"));
  ASSERT_TRUE(CreateDirectory(target2));
  // TODO(https://crbug.com/1432064): Expect that no events are fired.

  ASSERT_TRUE(DeleteFile(symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(CreateSymbolicLink(target2, symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create a file in target2.
  FilePath target2_file(target2.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target2_file, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(symlink);
  }
  delegate.RunUntilEventsMatch(event_expecter);
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

TEST_F(FilePathWatcherTest, MoveChild) {
  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  AccumulatingEventExpecter file_event_expecter, subdir_event_expecter;
  FilePath source_dir(temp_dir_.GetPath().AppendASCII("source"));
  FilePath source_subdir(source_dir.AppendASCII("subdir"));
  FilePath source_file(source_subdir.AppendASCII("file"));
  FilePath dest_dir(temp_dir_.GetPath().AppendASCII("dest"));
  FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  FilePath dest_file(dest_subdir.AppendASCII("file"));

  // Setup a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  ASSERT_TRUE(SetupWatch(dest_file, &file_watcher, &file_delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(dest_subdir, &subdir_watcher, &subdir_delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(Move(source_dir, dest_dir));
  file_event_expecter.AddExpectedEventForPath(dest_file);
  subdir_event_expecter.AddExpectedEventForPath(dest_subdir);
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);
}

// Verify that changing attributes on a file is caught
#if BUILDFLAG(IS_ANDROID)
// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#define FileAttributesChanged DISABLED_FileAttributesChanged
#endif  // BUILDFLAG(IS_ANDROID)
TEST_F(FilePathWatcherTest, FileAttributesChanged) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(MakeFileUnreadable(test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Verify that creating a symlink is caught.
TEST_F(FilePathWatcherTest, CreateLink) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is created.
  // Note that test_file() doesn't have to exist.
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that deleting a symlink is caught.
TEST_F(FilePathWatcherTest, DeleteLink) {
  // Unfortunately this test case only works if the link target exists.
  // TODO(craig) fix this as part of crbug.com/91561.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is deleted.
  ASSERT_TRUE(DeleteFile(test_link()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that modifying a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, ModifiedLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that creating a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, CreateTargetLinkedFile) {
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is created.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_link());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that deleting a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, DeleteTargetLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is deleted.
  ASSERT_TRUE(DeleteFile(test_file()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file whose parent directory is a link that
// doesn't exist yet works if the symlink is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart1) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // dir/file should exist.
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  // Note that we are watching dir.lnk/file which doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  VLOG(1) << "Waiting for link creation";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file creation + modification";
  // TODO(https://crbug.com/1432064): Should this fire two events on inotify?
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file whose parent directory is a
// dangling symlink works if the directory is created eventually.
// TODO(https://crbug.com/1432064): Add test coverage for symlinked file
// creation independent of a corresponding write.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart2) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;
  // AccumulatingEventExpecter event_expecter;
  // Now create the link from dir.lnk pointing to dir but
  // neither dir nor dir/file exist yet.
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(dir));
  // TODO(https://crbug.com/1432064): Expect that no events are fired.

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file with a symlink on the path
// to the file works.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart3) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file but the file doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(linkfile);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Regression tests that FilePathWatcherImpl does not leave its reference in
// `g_inotify_reader` due to a race in recursive watch.
// See https://crbug.com/990004.
TEST_F(FilePathWatcherTest, RacyRecursiveWatch) {
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    GTEST_SKIP();
  }

  FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));

  // Create and delete many subdirs. 20 is an arbitrary number big enough
  // to have more chances to make FilePathWatcherImpl leak watchers.
  std::vector<FilePath> subdirs;
  for (int i = 0; i < 20; ++i) {
    subdirs.emplace_back(dir.AppendASCII(StringPrintf("subdir_%d", i)));
  }

  Thread subdir_updater("SubDir Updater");
  ASSERT_TRUE(subdir_updater.Start());

  auto subdir_update_task = BindLambdaForTesting([&]() {
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
        BindLambdaForTesting([&](const FilePath& path, bool error) {
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
  ASSERT_FALSE(
      watcher->Watch(test_file(), FilePathWatcher::Type::kNonRecursive,
                     BindLambdaForTesting([&](const FilePath& path,
                                              bool error) { ADD_FAILURE(); })));

  // Triggers update but callback should not be invoked.
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  // Ensures that the callback did not happen.
  RunLoop().RunUntilIdle();
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

    RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        BindLambdaForTesting([&](const FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error) {
            return;
          }

          error_callback_called = true;

          if (callback_type == kDestroyWatcher) {
            watcher.reset();
          }

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
    RunLoop().RunUntilIdle();
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

    RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        BindLambdaForTesting([&](const FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error) {
            return;
          }

          error_callback_called = true;

          if (callback_type == kDestroyWatcher) {
            watcher.reset();
          }

          run_loop.Quit();
        });
    ASSERT_TRUE(watcher->Watch(dir, FilePathWatcher::Type::kRecursive,
                               watcher_callback));

    constexpr size_t kMaxLimit = 10u;
    ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(
        kMaxLimit);

    // Triggers updates and over limit.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      FilePath subdir = dir.AppendASCII(StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(CreateDirectory(subdir));
    }

    run_loop.Run();

    // More update but no more callback should happen.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      FilePath subdir = dir.AppendASCII(StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(DeleteFile(subdir));
    }
    RunLoop().RunUntilIdle();
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

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatchWithOptions(watched_folder, &directory_watcher,
                                    &delegate,
                                    {.type = FilePathWatcher::Type::kRecursive,
                                     .report_modified_path = true}));

  // Triggers two events:
  // create on /watched_folder/file.
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_RecursiveInNestedFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath subfolder(watched_folder.AppendASCII("subfolder"));
  FilePath file(subfolder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatchWithOptions(watched_folder, &directory_watcher,
                                    &delegate,
                                    {.type = FilePathWatcher::Type::kRecursive,
                                     .report_modified_path = true}));

  // Expects create on /watched_folder/subfolder.
  ASSERT_TRUE(CreateDirectory(subfolder));
  event_expecter.AddExpectedEventForPath(subfolder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Triggers two events:
  // create on /watched_folder/subfolder/file.
  // modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/subfolder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/subfolder.
  ASSERT_TRUE(DeleteFile(subfolder));
  event_expecter.AddExpectedEventForPath(subfolder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers two events:
  // create on /watched_folder/file.
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveRemoveEnclosingFolder) {
  FilePathWatcher directory_watcher;
  FilePath root_folder(temp_dir_.GetPath().AppendASCII("root_folder"));
  FilePath folder(root_folder.AppendASCII("folder"));
  FilePath watched_folder(folder.AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));
  ASSERT_TRUE(WriteFile(file, "test"));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers three events:
  // delete on /watched_folder/file.
  // delete on /watched_folder twice.
  // TODO(https://crbug.com/1432044): Figure out why duplicate events are fired
  // on `watched_folder`.
  ASSERT_TRUE(DeletePathRecursively(folder));
  event_expecter.AddExpectedEventForPath(file);
  event_expecter.AddExpectedEventForPath(watched_folder);
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_RecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kRecursive}));

  // Triggers two events:
  // create on /watched_folder.
  // modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(watched_folder);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  FilePath watched_folder(temp_dir_.GetPath().AppendASCII("watched_folder"));
  FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive}));

  // Triggers two events:
  // Expects create /watched_folder.
  // Expects modify /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(watched_folder);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

namespace {

enum Permission { Read, Write, Execute };

#if BUILDFLAG(IS_APPLE)
bool ChangeFilePermissions(const FilePath& path, Permission perm, bool allow) {
  struct stat stat_buf;

  if (stat(path.value().c_str(), &stat_buf) != 0) {
    return false;
  }

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

}  // namespace

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
  ASSERT_TRUE(CreateDirectory(test_dir1));
  ASSERT_TRUE(CreateDirectory(test_dir2));
  ASSERT_TRUE(WriteFile(test_file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // We should not get notified in this case as it hasn't affected our ability
  // to access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, false));
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, true));
  // TODO(https://crbug.com/1432064): Expect that no events are fired.

  // We should get notified in this case because filepathwatcher can no
  // longer access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, false));
  event_expecter.AddExpectedEventForPath(test_file);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, true));
  // TODO(https://crbug.com/1432064): Expect that no events are fired.
}

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)

// Fail fast if trying to trivially watch a non-existent item.
TEST_F(FilePathWatcherTest, TrivialNoDir) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath non_existent = tmp_dir.Append(FILE_PATH_LITERAL("nope"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_FALSE(SetupWatch(non_existent, &watcher, &delegate,
                          FilePathWatcher::Type::kTrivial));
}

// Succeed starting a watch on a directory.
TEST_F(FilePathWatcherTest, TrivialDirStart) {
  const FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));
}

// Observe a change on a directory
TEST_F(FilePathWatcherTest, TrivialDirChange) {
  const FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(TouchFile(tmp_dir, Time::Now(), Time::Now()));
  event_expecter.AddExpectedEventForPath(tmp_dir);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Observe no change when a parent is modified.
TEST_F(FilePathWatcherTest, TrivialParentDirChange) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath sub_dir1 = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));
  const FilePath sub_dir2 = sub_dir1.Append(FILE_PATH_LITERAL("subdir_redux"));

  ASSERT_TRUE(CreateDirectory(sub_dir1));
  ASSERT_TRUE(CreateDirectory(sub_dir2));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(sub_dir2, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  // There should be no notification for a change to |sub_dir2|'s parent.
  ASSERT_TRUE(Move(sub_dir1, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  delegate.RunUntilEventsMatch(event_expecter);
}

// Do not crash when a directory is moved; https://crbug.com/1156603.
TEST_F(FilePathWatcherTest, TrivialDirMove) {
  const FilePath tmp_dir = temp_dir_.GetPath();
  const FilePath sub_dir = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));

  ASSERT_TRUE(CreateDirectory(sub_dir));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(sub_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(Move(sub_dir, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  event_expecter.AddExpectedEventForPath(sub_dir, /**error=*/true);
  delegate.RunUntilEventsMatch(event_expecter);
}

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace base
