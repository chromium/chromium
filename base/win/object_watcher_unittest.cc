// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/object_watcher.h"

#include <windows.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

class QuitDelegate : public ObjectWatcher::Delegate {
 public:
  explicit QuitDelegate(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnObjectSignaled(HANDLE object) override {
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

class DecrementCountDelegate : public ObjectWatcher::Delegate {
 public:
  explicit DecrementCountDelegate(int* counter) : counter_(counter) {}
  void OnObjectSignaled(HANDLE object) override { --(*counter_); }

 private:
  raw_ptr<int> counter_;
};

void RunTest_BasicSignal(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(main_thread_type);

  ObjectWatcher watcher;
  EXPECT_FALSE(watcher.IsWatching());

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

  base::RunLoop loop;
  QuitDelegate delegate(loop.QuitWhenIdleClosure());
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_EQ(event, watcher.GetWatchedObject());

  SetEvent(event);

  loop.Run();

  EXPECT_FALSE(watcher.IsWatching());
  CloseHandle(event);
}

void RunTest_BasicCancel(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(main_thread_type);

  ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

  base::RunLoop loop;
  QuitDelegate delegate(loop.QuitWhenIdleClosure());
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  watcher.StopWatching();

  CloseHandle(event);
}

void RunTest_CancelAfterSet(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(main_thread_type);

  ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);
  base::RunLoop loop;
  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  SetEvent(event);

  // Let the background thread do its business
  Sleep(30);

  watcher.StopWatching();

  loop.RunUntilIdle();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);

  CloseHandle(event);
}

void RunTest_SignalBeforeWatch(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(main_thread_type);

  ObjectWatcher watcher;

  // A manual-reset event that is signaled before we begin watching.
  HANDLE event = CreateEvent(nullptr, TRUE, TRUE, nullptr);

  base::RunLoop loop;
  QuitDelegate delegate(loop.QuitWhenIdleClosure());
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  loop.Run();

  EXPECT_FALSE(watcher.IsWatching());
  CloseHandle(event);
}

void RunTest_OutlivesTaskEnvironment(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  // Simulate a task environment that dies before an ObjectWatcher.  This
  // ordinarily doesn't happen when people use the Thread class, but it can
  // happen when people use the Singleton pattern or atexit.
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);  // not signaled
  {
    ObjectWatcher watcher;
    {
      test::TaskEnvironment task_environment(main_thread_type);

      base::RunLoop loop;
      QuitDelegate delegate(loop.QuitWhenIdleClosure());
      watcher.StartWatchingOnce(event, &delegate);
    }
  }
  CloseHandle(event);
}

class QuitAfterMultipleDelegate : public ObjectWatcher::Delegate {
 public:
  QuitAfterMultipleDelegate(HANDLE event,
                            int iterations,
                            base::OnceClosure quit_closure)
      : event_(event),
        iterations_(iterations),
        quit_closure_(std::move(quit_closure)) {}
  void OnObjectSignaled(HANDLE object) override {
    if (--iterations_) {
      SetEvent(event_);
    } else {
      std::move(quit_closure_).Run();
    }
  }

 private:
  HANDLE event_;
  int iterations_;
  base::OnceClosure quit_closure_;
};

void RunTest_ExecuteMultipleTimes(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(main_thread_type);

  ObjectWatcher watcher;
  EXPECT_FALSE(watcher.IsWatching());

  // An auto-reset event that is not yet signaled.
  HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  base::RunLoop loop;
  QuitAfterMultipleDelegate delegate(event, 2, loop.QuitWhenIdleClosure());
  bool ok = watcher.StartWatchingMultipleTimes(event, &delegate);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_EQ(event, watcher.GetWatchedObject());

  SetEvent(event);

  loop.Run();

  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_TRUE(watcher.StopWatching());
  CloseHandle(event);
}

}  // namespace

//-----------------------------------------------------------------------------

TEST(ObjectWatcherTest, BasicSignal) {
  RunTest_BasicSignal(test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_BasicSignal(test::TaskEnvironment::MainThreadType::IO);
  RunTest_BasicSignal(test::TaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, BasicCancel) {
  RunTest_BasicCancel(test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_BasicCancel(test::TaskEnvironment::MainThreadType::IO);
  RunTest_BasicCancel(test::TaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, CancelAfterSet) {
  RunTest_CancelAfterSet(test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_CancelAfterSet(test::TaskEnvironment::MainThreadType::IO);
  RunTest_CancelAfterSet(test::TaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, SignalBeforeWatch) {
  RunTest_SignalBeforeWatch(test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_SignalBeforeWatch(test::TaskEnvironment::MainThreadType::IO);
  RunTest_SignalBeforeWatch(test::TaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, OutlivesTaskEnvironment) {
  RunTest_OutlivesTaskEnvironment(
      test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_OutlivesTaskEnvironment(test::TaskEnvironment::MainThreadType::IO);
  RunTest_OutlivesTaskEnvironment(test::TaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, ExecuteMultipleTimes) {
  RunTest_ExecuteMultipleTimes(test::TaskEnvironment::MainThreadType::DEFAULT);
  RunTest_ExecuteMultipleTimes(test::TaskEnvironment::MainThreadType::IO);
  RunTest_ExecuteMultipleTimes(test::TaskEnvironment::MainThreadType::UI);
}

}  // namespace win
}  // namespace base
