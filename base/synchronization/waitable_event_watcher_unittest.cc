// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// The main thread types on which each waitable event should be tested.
const test::TaskEnvironment::MainThreadType testing_main_threads[] = {
    test::TaskEnvironment::MainThreadType::DEFAULT,
    test::TaskEnvironment::MainThreadType::IO,
#if !BUILDFLAG(IS_IOS)  // iOS does not allow direct running of the UI loop.
    test::TaskEnvironment::MainThreadType::UI,
#endif
};

void QuitWhenSignaled(base::OnceClosure quit_closure, WaitableEvent* event) {
  std::move(quit_closure).Run();
}

class DecrementCountContainer {
 public:
  explicit DecrementCountContainer(int* counter) : counter_(counter) {}
  void OnWaitableEventSignaled(WaitableEvent* object) {
    // NOTE: |object| may be already deleted.
    --(*counter_);
  }

 private:
  raw_ptr<int> counter_;
};

}  // namespace

class WaitableEventWatcherTest
    : public testing::TestWithParam<test::TaskEnvironment::MainThreadType> {};

TEST_P(WaitableEventWatcherTest, BasicSignalManual) {
  test::TaskEnvironment task_environment(GetParam());
  base::RunLoop loop;
  // A manual-reset event that is not yet signaled.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventWatcher watcher;
  watcher.StartWatching(&event,
                        BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
                        SequencedTaskRunner::GetCurrentDefault());

  event.Signal();

  loop.Run();

  EXPECT_TRUE(event.IsSignaled());
}

TEST_P(WaitableEventWatcherTest, BasicSignalAutomatic) {
  test::TaskEnvironment task_environment(GetParam());

  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventWatcher watcher;
  base::RunLoop loop;
  watcher.StartWatching(&event,
                        BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
                        SequencedTaskRunner::GetCurrentDefault());

  event.Signal();

  loop.Run();

  // The WaitableEventWatcher consumes the event signal.
  EXPECT_FALSE(event.IsSignaled());
}

TEST_P(WaitableEventWatcherTest, BasicCancel) {
  test::TaskEnvironment task_environment(GetParam());

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventWatcher watcher;
  watcher.StartWatching(&event, DoNothing(),
                        SequencedTaskRunner::GetCurrentDefault());

  watcher.StopWatching();
}

TEST_P(WaitableEventWatcherTest, CancelAfterSet) {
  test::TaskEnvironment task_environment(GetParam());

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventWatcher watcher;

  int counter = 1;
  DecrementCountContainer delegate(&counter);
  WaitableEventWatcher::EventCallback callback = BindOnce(
      &DecrementCountContainer::OnWaitableEventSignaled, Unretained(&delegate));
  watcher.StartWatching(&event, std::move(callback),
                        SequencedTaskRunner::GetCurrentDefault());

  event.Signal();

  // Let the background thread do its business
  PlatformThread::Sleep(Milliseconds(30));

  watcher.StopWatching();

  RunLoop().RunUntilIdle();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
}

TEST_P(WaitableEventWatcherTest, OutlivesTaskEnvironment) {
  // Simulate a task environment that dies before an WaitableEventWatcher.  This
  // ordinarily doesn't happen when people use the Thread class, but it can
  // happen when people use the Singleton pattern or atexit.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  {
    std::unique_ptr<WaitableEventWatcher> watcher;
    {
      test::TaskEnvironment task_environment(GetParam());
      watcher = std::make_unique<WaitableEventWatcher>();

      watcher->StartWatching(&event, DoNothing(),
                             SequencedTaskRunner::GetCurrentDefault());
    }
  }
}

TEST_P(WaitableEventWatcherTest, SignaledAtStartManual) {
  test::TaskEnvironment task_environment(GetParam());
  base::RunLoop loop;
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::SIGNALED);

  WaitableEventWatcher watcher;
  watcher.StartWatching(&event,
                        BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
                        SequencedTaskRunner::GetCurrentDefault());

  loop.Run();

  EXPECT_TRUE(event.IsSignaled());
}

TEST_P(WaitableEventWatcherTest, SignaledAtStartAutomatic) {
  test::TaskEnvironment task_environment(GetParam());
  base::RunLoop loop;
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::SIGNALED);

  WaitableEventWatcher watcher;
  watcher.StartWatching(&event,
                        BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
                        SequencedTaskRunner::GetCurrentDefault());

  loop.Run();

  // The watcher consumes the event signal.
  EXPECT_FALSE(event.IsSignaled());
}

TEST_P(WaitableEventWatcherTest, StartWatchingInCallback) {
  test::TaskEnvironment task_environment(GetParam());

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventWatcher watcher;
  base::RunLoop loop;
  watcher.StartWatching(&event, BindLambdaForTesting([&](WaitableEvent* event) {
    // |event| is manual, so the second watcher will run
    // immediately.
    watcher.StartWatching(
        event, BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
        SequencedTaskRunner::GetCurrentDefault());
  }),
                        SequencedTaskRunner::GetCurrentDefault());

  event.Signal();

  loop.Run();
}

TEST_P(WaitableEventWatcherTest, MultipleWatchersManual) {
  test::TaskEnvironment task_environment(GetParam());

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  int watcher1_counter = 0;
  int watcher2_counter = 0;

  int total_counter = 0;

  RunLoop run_loop;

  auto callback = [&run_loop, &total_counter](int* watcher_counter,
                                              WaitableEvent*) {
    ++(*watcher_counter);
    if (++total_counter == 2) {
      run_loop.Quit();
    }
  };

  WaitableEventWatcher watcher1;
  watcher1.StartWatching(
      &event,
      BindOnce(BindLambdaForTesting(callback), Unretained(&watcher1_counter)),
      SequencedTaskRunner::GetCurrentDefault());

  WaitableEventWatcher watcher2;
  watcher2.StartWatching(
      &event,
      BindOnce(BindLambdaForTesting(callback), Unretained(&watcher2_counter)),
      SequencedTaskRunner::GetCurrentDefault());

  event.Signal();
  run_loop.Run();

  EXPECT_EQ(1, watcher1_counter);
  EXPECT_EQ(1, watcher2_counter);
  EXPECT_EQ(2, total_counter);
  EXPECT_TRUE(event.IsSignaled());
}

// Tests that only one async waiter gets called back for an auto-reset event.
TEST_P(WaitableEventWatcherTest, MultipleWatchersAutomatic) {
  test::TaskEnvironment task_environment(GetParam());

  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  int counter1 = 0;
  int counter2 = 0;

  auto callback = [](RunLoop** run_loop, int* counter, WaitableEvent* event) {
    ++(*counter);
    (*run_loop)->QuitWhenIdle();
  };

  // The same RunLoop instance cannot be Run more than once, and it is
  // undefined which watcher will get called back first. Have the callback
  // dereference this pointer to quit the loop, which will be updated on each
  // Run.
  RunLoop* current_run_loop;

  WaitableEventWatcher watcher1;
  watcher1.StartWatching(
      &event,
      BindOnce(callback, Unretained(&current_run_loop), Unretained(&counter1)),
      SequencedTaskRunner::GetCurrentDefault());

  WaitableEventWatcher watcher2;
  watcher2.StartWatching(
      &event,
      BindOnce(callback, Unretained(&current_run_loop), Unretained(&counter2)),
      SequencedTaskRunner::GetCurrentDefault());

  event.Signal();
  {
    RunLoop run_loop;
    current_run_loop = &run_loop;
    run_loop.Run();
  }

  // Only one of the waiters should have been signaled.
  EXPECT_TRUE((counter1 == 1) ^ (counter2 == 1));

  EXPECT_FALSE(event.IsSignaled());

  event.Signal();
  {
    RunLoop run_loop;
    current_run_loop = &run_loop;
    run_loop.Run();
  }

  EXPECT_FALSE(event.IsSignaled());

  // The other watcher should have been signaled.
  EXPECT_EQ(1, counter1);
  EXPECT_EQ(1, counter2);
}

// To help detect errors around deleting WaitableEventWatcher, an additional
// bool parameter is used to test sleeping between watching and deletion.
class WaitableEventWatcherDeletionTest
    : public testing::TestWithParam<
          std::tuple<test::TaskEnvironment::MainThreadType, bool>> {};

TEST_P(WaitableEventWatcherDeletionTest, DeleteUnder) {
  auto [main_thread_type, delay_after_delete] = GetParam();

  // Delete the WaitableEvent out from under the Watcher. This is explictly
  // allowed by the interface.

  test::TaskEnvironment task_environment(main_thread_type);

  {
    WaitableEventWatcher watcher;

    auto* event = new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED);

    watcher.StartWatching(event, DoNothing(),
                          SequencedTaskRunner::GetCurrentDefault());

    if (delay_after_delete) {
      // On Windows that sleep() improves the chance to catch some problems.
      // It postpones the dtor |watcher| (which immediately cancel the waiting)
      // and gives some time to run to a created background thread.
      // Unfortunately, that thread is under OS control and we can't
      // manipulate it directly.
      PlatformThread::Sleep(Milliseconds(30));
    }

    delete event;
  }
}

TEST_P(WaitableEventWatcherDeletionTest, SignalAndDelete) {
  auto [main_thread_type, delay_after_delete] = GetParam();

  // Signal and immediately delete the WaitableEvent out from under the Watcher.

  test::TaskEnvironment task_environment(main_thread_type);

  {
    base::RunLoop loop;
    WaitableEventWatcher watcher;

    auto event = std::make_unique<WaitableEvent>(
        WaitableEvent::ResetPolicy::AUTOMATIC,
        WaitableEvent::InitialState::NOT_SIGNALED);

    watcher.StartWatching(
        event.get(), BindOnce(&QuitWhenSignaled, loop.QuitWhenIdleClosure()),
        SequencedTaskRunner::GetCurrentDefault());
    event->Signal();
    event.reset();

    if (delay_after_delete) {
      // On Windows that sleep() improves the chance to catch some problems.
      // It postpones the dtor |watcher| (which immediately cancel the waiting)
      // and gives some time to run to a created background thread.
      // Unfortunately, that thread is under OS control and we can't
      // manipulate it directly.
      PlatformThread::Sleep(Milliseconds(30));
    }

    // Wait for the watcher callback.
    loop.Run();
  }
}

// Tests deleting the WaitableEventWatcher between signaling the event and
// when the callback should be run.
TEST_P(WaitableEventWatcherDeletionTest, DeleteWatcherBeforeCallback) {
  auto [main_thread_type, delay_after_delete] = GetParam();

  test::TaskEnvironment task_environment(main_thread_type);
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      SingleThreadTaskRunner::GetCurrentDefault();

  // Flag used to esnure that the |watcher_callback| never runs.
  bool did_callback = false;

  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  auto watcher = std::make_unique<WaitableEventWatcher>();

  // Queue up a series of tasks:
  // 1. StartWatching the WaitableEvent
  // 2. Signal the event (which will result in another task getting posted to
  //    the |task_runner|)
  // 3. Delete the WaitableEventWatcher
  // 4. WaitableEventWatcher callback should run (from #2)

  WaitableEventWatcher::EventCallback watcher_callback = BindOnce(
      [](bool* did_callback, WaitableEvent*) {
        *did_callback = true;
      },
      Unretained(&did_callback));

  task_runner->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), Unretained(&event),
                          std::move(watcher_callback), task_runner));
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  task_runner->DeleteSoon(FROM_HERE, std::move(watcher));
  if (delay_after_delete) {
    task_runner->PostTask(FROM_HERE,
                          BindOnce(&PlatformThread::Sleep, Milliseconds(30)));
  }

  RunLoop().RunUntilIdle();

  EXPECT_FALSE(did_callback);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WaitableEventWatcherTest,
                         testing::ValuesIn(testing_main_threads));

INSTANTIATE_TEST_SUITE_P(
    All,
    WaitableEventWatcherDeletionTest,
    testing::Combine(testing::ValuesIn(testing_main_threads), testing::Bool()));

}  // namespace base
