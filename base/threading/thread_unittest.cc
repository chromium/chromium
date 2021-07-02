// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/task_executor.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Thread;
using ::testing::NotNull;

typedef PlatformTest ThreadTest;

namespace {

void ToggleValue(bool* value) {
  ANNOTATE_BENIGN_RACE(value, "Test-only data race on boolean "
                       "in base/thread_unittest");
  *value = !*value;
}

class SleepInsideInitThread : public Thread {
 public:
  SleepInsideInitThread() : Thread("none") {
    init_called_ = false;
    ANNOTATE_BENIGN_RACE(
        this, "Benign test-only data race on vptr - http://crbug.com/98219");
  }
  ~SleepInsideInitThread() override { Stop(); }

  void Init() override {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(500));
    init_called_ = true;
  }
  bool InitCalled() { return init_called_; }

 private:
  bool init_called_;

  DISALLOW_COPY_AND_ASSIGN(SleepInsideInitThread);
};

enum ThreadEvent {
  // Thread::Init() was called.
  THREAD_EVENT_INIT = 0,

  // The MessageLoop for the thread was deleted.
  THREAD_EVENT_MESSAGE_LOOP_DESTROYED,

  // Thread::CleanUp() was called.
  THREAD_EVENT_CLEANUP,

  // Keep at end of list.
  THREAD_NUM_EVENTS
};

typedef std::vector<ThreadEvent> EventList;

class CaptureToEventList : public Thread {
 public:
  // This Thread pushes events into the vector |event_list| to show
  // the order they occured in. |event_list| must remain valid for the
  // lifetime of this thread.
  explicit CaptureToEventList(EventList* event_list)
      : Thread("none"),
        event_list_(event_list) {
  }

  ~CaptureToEventList() override { Stop(); }

  void Init() override { event_list_->push_back(THREAD_EVENT_INIT); }

  void CleanUp() override { event_list_->push_back(THREAD_EVENT_CLEANUP); }

 private:
  EventList* event_list_;

  DISALLOW_COPY_AND_ASSIGN(CaptureToEventList);
};

// Observer that writes a value into |event_list| when a message loop has been
// destroyed.
class CapturingDestructionObserver
    : public base::CurrentThread::DestructionObserver {
 public:
  // |event_list| must remain valid throughout the observer's lifetime.
  explicit CapturingDestructionObserver(EventList* event_list)
      : event_list_(event_list) {
  }

  // DestructionObserver implementation:
  void WillDestroyCurrentMessageLoop() override {
    event_list_->push_back(THREAD_EVENT_MESSAGE_LOOP_DESTROYED);
    event_list_ = nullptr;
  }

 private:
  EventList* event_list_;

  DISALLOW_COPY_AND_ASSIGN(CapturingDestructionObserver);
};

// Task that adds a destruction observer to the current message loop.
void RegisterDestructionObserver(
    base::CurrentThread::DestructionObserver* observer) {
  base::CurrentThread::Get()->AddDestructionObserver(observer);
}

// Task that calls GetThreadId() of |thread|, stores the result into |id|, then
// signal |event|.
void ReturnThreadId(base::Thread* thread,
                    base::PlatformThreadId* id,
                    base::WaitableEvent* event) {
  *id = thread->GetThreadId();
  event->Signal();
}

}  // namespace

TEST_F(ThreadTest, StartWithOptions_StackSize) {
  Thread a("StartWithStackSize");
  // Ensure that the thread can work with only 12 kb and still process a
  // message. At the same time, we should scale with the bitness of the system
  // where 12 kb is definitely not enough.
  // 12 kb = 3072 Slots on a 32-bit system, so we'll scale based off of that.
  Thread::Options options;
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
  // ASan bloats the stack variables and overflows the 3072 slot stack. Some
  // debug builds also grow the stack too much.
  options.stack_size = 2 * 3072 * sizeof(uintptr_t);
#else
  options.stack_size = 3072 * sizeof(uintptr_t);
#endif
  EXPECT_TRUE(a.StartWithOptions(std::move(options)));
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  a.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
  event.Wait();
}

// Intentional test-only race for otherwise untestable code, won't fix.
// https://crbug.com/634383
#if !defined(THREAD_SANITIZER)
TEST_F(ThreadTest, StartWithOptions_NonJoinable) {
  Thread* a = new Thread("StartNonJoinable");
  // Non-joinable threads have to be leaked for now (see
  // Thread::Options::joinable for details).
  ANNOTATE_LEAKING_OBJECT_PTR(a);

  Thread::Options options;
  options.joinable = false;
  EXPECT_TRUE(a->StartWithOptions(std::move(options)));
  EXPECT_TRUE(a->task_runner());
  EXPECT_TRUE(a->IsRunning());

  // Without this call this test is racy. The above IsRunning() succeeds because
  // of an early-return condition while between Start() and StopSoon(), after
  // invoking StopSoon() below this early-return condition is no longer
  // satisfied and the real |is_running_| bit has to be checked. It could still
  // be false if the message loop hasn't started for real in practice. This is
  // only a requirement for this test because the non-joinable property forces
  // it to use StopSoon() and not wait for a complete Stop().
  EXPECT_TRUE(a->WaitUntilThreadStarted());

  // Make the thread block until |block_event| is signaled.
  base::WaitableEvent block_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  a->task_runner()->PostTask(FROM_HERE,
                             base::BindOnce(&base::WaitableEvent::Wait,
                                            base::Unretained(&block_event)));

  a->StopSoon();
  EXPECT_TRUE(a->IsRunning());

  // Unblock the task and give a bit of extra time to unwind QuitWhenIdle().
  block_event.Signal();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));

  // The thread should now have stopped on its own.
  EXPECT_FALSE(a->IsRunning());
}
#endif

TEST_F(ThreadTest, TwoTasksOnJoinableThread) {
  bool was_invoked = false;
  {
    Thread a("TwoTasksOnJoinableThread");
    EXPECT_TRUE(a.Start());
    EXPECT_TRUE(a.task_runner());

    // Test that all events are dispatched before the Thread object is
    // destroyed.  We do this by dispatching a sleep event before the
    // event that will toggle our sentinel value.
    a.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(static_cast<void (*)(base::TimeDelta)>(
                                      &base::PlatformThread::Sleep),
                                  base::TimeDelta::FromMilliseconds(20)));
    a.task_runner()->PostTask(FROM_HERE,
                              base::BindOnce(&ToggleValue, &was_invoked));
  }
  EXPECT_TRUE(was_invoked);
}

TEST_F(ThreadTest, DestroyWhileRunningIsSafe) {
  Thread a("DestroyWhileRunningIsSafe");
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.WaitUntilThreadStarted());
}

// TODO(gab): Enable this test when destroying a non-joinable Thread instance
// is supported (proposal @ https://crbug.com/629139#c14).
TEST_F(ThreadTest, DISABLED_DestroyWhileRunningNonJoinableIsSafe) {
  {
    Thread a("DestroyWhileRunningNonJoinableIsSafe");
    Thread::Options options;
    options.joinable = false;
    EXPECT_TRUE(a.StartWithOptions(std::move(options)));
    EXPECT_TRUE(a.WaitUntilThreadStarted());
  }

  // Attempt to catch use-after-frees from the non-joinable thread in the
  // scope of this test if any.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
}

TEST_F(ThreadTest, StopSoon) {
  Thread a("StopSoon");
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());
  a.StopSoon();
  a.Stop();
  EXPECT_FALSE(a.task_runner());
  EXPECT_FALSE(a.IsRunning());
}

TEST_F(ThreadTest, StopTwiceNop) {
  Thread a("StopTwiceNop");
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());
  a.StopSoon();
  // Calling StopSoon() a second time should be a nop.
  a.StopSoon();
  a.Stop();
  // Same with Stop().
  a.Stop();
  EXPECT_FALSE(a.task_runner());
  EXPECT_FALSE(a.IsRunning());
  // Calling them when not running should also nop.
  a.StopSoon();
  a.Stop();
}

// TODO(gab): Enable this test in conjunction with re-enabling the sequence
// check in Thread::Stop() as part of http://crbug.com/629139.
TEST_F(ThreadTest, DISABLED_StopOnNonOwningThreadIsDeath) {
  Thread a("StopOnNonOwningThreadDeath");
  EXPECT_TRUE(a.StartAndWaitForTesting());

  Thread b("NonOwningThread");
  b.Start();
  EXPECT_DCHECK_DEATH({
    // Stopping |a| on |b| isn't allowed.
    b.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Thread::Stop, base::Unretained(&a)));
    // Block here so the DCHECK on |b| always happens in this scope.
    base::PlatformThread::Sleep(base::TimeDelta::Max());
  });
}

TEST_F(ThreadTest, TransferOwnershipAndStop) {
  std::unique_ptr<Thread> a =
      std::make_unique<Thread>("TransferOwnershipAndStop");
  EXPECT_TRUE(a->StartAndWaitForTesting());
  EXPECT_TRUE(a->IsRunning());

  Thread b("TakingOwnershipThread");
  b.Start();

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // a->DetachFromSequence() should allow |b| to use |a|'s Thread API.
  a->DetachFromSequence();
  b.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<Thread> thread_to_stop,
                        base::WaitableEvent* event_to_signal) -> void {
                       thread_to_stop->Stop();
                       event_to_signal->Signal();
                     },
                     std::move(a), base::Unretained(&event)));

  event.Wait();
}

TEST_F(ThreadTest, StartTwice) {
  Thread a("StartTwice");

  EXPECT_FALSE(a.task_runner());
  EXPECT_FALSE(a.IsRunning());

  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());

  a.Stop();
  EXPECT_FALSE(a.task_runner());
  EXPECT_FALSE(a.IsRunning());

  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());

  a.Stop();
  EXPECT_FALSE(a.task_runner());
  EXPECT_FALSE(a.IsRunning());
}

// Intentional test-only race for otherwise untestable code, won't fix.
// https://crbug.com/634383
#if !defined(THREAD_SANITIZER)
TEST_F(ThreadTest, StartTwiceNonJoinableNotAllowed) {
  LOG(ERROR) << __FUNCTION__;
  Thread* a = new Thread("StartTwiceNonJoinable");
  // Non-joinable threads have to be leaked for now (see
  // Thread::Options::joinable for details).
  ANNOTATE_LEAKING_OBJECT_PTR(a);

  Thread::Options options;
  options.joinable = false;
  EXPECT_TRUE(a->StartWithOptions(std::move(options)));
  EXPECT_TRUE(a->task_runner());
  EXPECT_TRUE(a->IsRunning());

  // Signaled when last task on |a| is processed.
  base::WaitableEvent last_task_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  a->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                base::Unretained(&last_task_event)));

  // StopSoon() is non-blocking, Yield() to |a|, wait for last task to be
  // processed and a little more for QuitWhenIdle() to unwind before considering
  // the thread "stopped".
  a->StopSoon();
  base::PlatformThread::YieldCurrentThread();
  last_task_event.Wait();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));

  // This test assumes that the above was sufficient to let the thread fully
  // stop.
  ASSERT_FALSE(a->IsRunning());

  // Restarting it should not be allowed.
  EXPECT_DCHECK_DEATH(a->Start());
}
#endif

TEST_F(ThreadTest, ThreadName) {
  Thread a("ThreadName");
  EXPECT_TRUE(a.Start());
  EXPECT_EQ("ThreadName", a.thread_name());
}

TEST_F(ThreadTest, ThreadId) {
  Thread a("ThreadId0");
  Thread b("ThreadId1");
  a.Start();
  b.Start();

  // Post a task that calls GetThreadId() on the created thread.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PlatformThreadId id_from_new_thread;
  a.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(ReturnThreadId, &a, &id_from_new_thread, &event));

  // Call GetThreadId() on the current thread before calling event.Wait() so
  // that this test can find a race issue with TSAN.
  base::PlatformThreadId id_from_current_thread = a.GetThreadId();

  // Check if GetThreadId() returns consistent value in both threads.
  event.Wait();
  EXPECT_EQ(id_from_current_thread, id_from_new_thread);

  // A started thread should have a valid ID.
  EXPECT_NE(base::kInvalidThreadId, a.GetThreadId());
  EXPECT_NE(base::kInvalidThreadId, b.GetThreadId());

  // Each thread should have a different thread ID.
  EXPECT_NE(a.GetThreadId(), b.GetThreadId());
}

TEST_F(ThreadTest, ThreadIdWithRestart) {
  Thread a("ThreadIdWithRestart");
  base::PlatformThreadId previous_id = base::kInvalidThreadId;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_TRUE(a.Start());
    base::PlatformThreadId current_id = a.GetThreadId();
    EXPECT_NE(previous_id, current_id);
    previous_id = current_id;
    a.Stop();
  }
}

// Make sure Init() is called after Start() and before
// WaitUntilThreadInitialized() returns.
TEST_F(ThreadTest, SleepInsideInit) {
  SleepInsideInitThread t;
  EXPECT_FALSE(t.InitCalled());
  t.StartAndWaitForTesting();
  EXPECT_TRUE(t.InitCalled());
}

// Make sure that the destruction sequence is:
//
//  (1) Thread::CleanUp()
//  (2) MessageLoop::~MessageLoop()
//      CurrentThread::DestructionObservers called.
TEST_F(ThreadTest, CleanUp) {
  EventList captured_events;
  CapturingDestructionObserver loop_destruction_observer(&captured_events);

  {
    // Start a thread which writes its event into |captured_events|.
    CaptureToEventList t(&captured_events);
    EXPECT_TRUE(t.Start());
    EXPECT_TRUE(t.task_runner());
    EXPECT_TRUE(t.IsRunning());

    // Register an observer that writes into |captured_events| once the
    // thread's message loop is destroyed.
    t.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RegisterDestructionObserver,
                       base::Unretained(&loop_destruction_observer)));

    // Upon leaving this scope, the thread is deleted.
  }

  // Check the order of events during shutdown.
  ASSERT_EQ(static_cast<size_t>(THREAD_NUM_EVENTS), captured_events.size());
  EXPECT_EQ(THREAD_EVENT_INIT, captured_events[0]);
  EXPECT_EQ(THREAD_EVENT_CLEANUP, captured_events[1]);
  EXPECT_EQ(THREAD_EVENT_MESSAGE_LOOP_DESTROYED, captured_events[2]);
}

TEST_F(ThreadTest, ThreadNotStarted) {
  Thread a("Inert");
  EXPECT_FALSE(a.task_runner());
}

TEST_F(ThreadTest, MultipleWaitUntilThreadStarted) {
  Thread a("MultipleWaitUntilThreadStarted");
  EXPECT_TRUE(a.Start());
  // It's OK to call WaitUntilThreadStarted() multiple times.
  EXPECT_TRUE(a.WaitUntilThreadStarted());
  EXPECT_TRUE(a.WaitUntilThreadStarted());
}

TEST_F(ThreadTest, FlushForTesting) {
  Thread a("FlushForTesting");

  // Flushing a non-running thread should be a no-op.
  a.FlushForTesting();

  ASSERT_TRUE(a.Start());

  // Flushing a thread with no tasks shouldn't block.
  a.FlushForTesting();

  constexpr base::TimeDelta kSleepPerTestTask =
      base::TimeDelta::FromMilliseconds(50);
  constexpr size_t kNumSleepTasks = 5;

  const base::TimeTicks ticks_before_post = base::TimeTicks::Now();

  for (size_t i = 0; i < kNumSleepTasks; ++i) {
    a.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::PlatformThread::Sleep, kSleepPerTestTask));
  }

  // All tasks should have executed, as reflected by the elapsed time.
  a.FlushForTesting();
  EXPECT_GE(base::TimeTicks::Now() - ticks_before_post,
            kNumSleepTasks * kSleepPerTestTask);

  a.Stop();

  // Flushing a stopped thread should be a no-op.
  a.FlushForTesting();
}

TEST_F(ThreadTest, GetTaskExecutorForCurrentThread) {
  Thread a("GetTaskExecutorForCurrentThread");
  ASSERT_TRUE(a.Start());

  base::WaitableEvent event;

  a.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_THAT(base::GetTaskExecutorForCurrentThread(), NotNull());
        event.Signal();
      }));

  event.Wait();
  a.Stop();
}

namespace {

class SequenceManagerThreadDelegate : public Thread::Delegate {
 public:
  SequenceManagerThreadDelegate()
      : sequence_manager_(
            base::sequence_manager::CreateUnboundSequenceManager()),
        task_queue_(
            sequence_manager_
                ->CreateTaskQueueWithType<base::sequence_manager::TaskQueue>(
                    base::sequence_manager::TaskQueue::Spec("default_tq"))) {
    sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
  }

  ~SequenceManagerThreadDelegate() override {}

  // Thread::Delegate:

  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override {
    return task_queue_->task_runner();
  }

  void BindToCurrentThread(base::TimerSlack timer_slack) override {
    sequence_manager_->BindToMessagePump(
        base::MessagePump::Create(base::MessagePumpType::DEFAULT));
    sequence_manager_->SetTimerSlack(timer_slack);
  }

 private:
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  scoped_refptr<base::sequence_manager::TaskQueue> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(SequenceManagerThreadDelegate);
};

}  // namespace

TEST_F(ThreadTest, ProvidedThreadDelegate) {
  Thread thread("ThreadDelegate");
  base::Thread::Options options;
  options.delegate = std::make_unique<SequenceManagerThreadDelegate>();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      options.delegate->GetDefaultTaskRunner();
  thread.StartWithOptions(std::move(options));

  base::WaitableEvent event;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();

  thread.Stop();
}
