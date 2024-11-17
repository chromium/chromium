// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

#if DCHECK_IS_ON()
#include "base/threading/thread_restrictions.h"
#endif

using ::testing::NotNull;

using ThreadTest = PlatformTest;

namespace base {
namespace {

void ToggleValue(bool* value) {
  ABSL_ANNOTATE_BENIGN_RACE(
      value, "Test-only data race on boolean in base/thread_unittest");
  *value = !*value;
}

class SleepInsideInitThread : public Thread {
 public:
  SleepInsideInitThread() : Thread("none") {
    init_called_ = false;
    ABSL_ANNOTATE_BENIGN_RACE(
        this, "Benign test-only data race on vptr - http://crbug.com/98219");
  }

  SleepInsideInitThread(const SleepInsideInitThread&) = delete;
  SleepInsideInitThread& operator=(const SleepInsideInitThread&) = delete;

  ~SleepInsideInitThread() override { Stop(); }

  void Init() override {
    PlatformThread::Sleep(Milliseconds(500));
    init_called_ = true;
  }
  bool InitCalled() { return init_called_; }

 private:
  bool init_called_;
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

using EventList = std::vector<ThreadEvent>;

class CaptureToEventList : public Thread {
 public:
  // This Thread pushes events into the vector |event_list| to show
  // the order they occured in. |event_list| must remain valid for the
  // lifetime of this thread.
  explicit CaptureToEventList(EventList* event_list)
      : Thread("none"), event_list_(event_list) {}

  CaptureToEventList(const CaptureToEventList&) = delete;
  CaptureToEventList& operator=(const CaptureToEventList&) = delete;

  ~CaptureToEventList() override { Stop(); }

  void Init() override { event_list_->push_back(THREAD_EVENT_INIT); }

  void CleanUp() override { event_list_->push_back(THREAD_EVENT_CLEANUP); }

 private:
  raw_ptr<EventList> event_list_;
};

// Observer that writes a value into |event_list| when a message loop has been
// destroyed.
class CapturingDestructionObserver : public CurrentThread::DestructionObserver {
 public:
  // |event_list| must remain valid throughout the observer's lifetime.
  explicit CapturingDestructionObserver(EventList* event_list)
      : event_list_(event_list) {}

  CapturingDestructionObserver(const CapturingDestructionObserver&) = delete;
  CapturingDestructionObserver& operator=(const CapturingDestructionObserver&) =
      delete;

  // DestructionObserver implementation:
  void WillDestroyCurrentMessageLoop() override {
    event_list_->push_back(THREAD_EVENT_MESSAGE_LOOP_DESTROYED);
    event_list_ = nullptr;
  }

 private:
  raw_ptr<EventList> event_list_;
};

// Task that adds a destruction observer to the current message loop.
void RegisterDestructionObserver(CurrentThread::DestructionObserver* observer) {
  CurrentThread::Get()->AddDestructionObserver(observer);
}

// Task that calls GetThreadId() of |thread|, stores the result into |id|, then
// signal |event|.
void ReturnThreadId(Thread* thread,
                    PlatformThreadId* id,
                    WaitableEvent* event) {
  *id = thread->GetThreadId();
  event->Signal();
}

}  // namespace

TEST_F(ThreadTest, StartWithOptions_StackSize) {
  // Ensure that the thread can work with a small stack and still process a
  // message. On a 32-bit system, a release build should be able to work with
  // 12 KiB.
  size_t num_slots = 12 * 1024 / 4;
  size_t slot_size = sizeof(char*);
  int additional_space = 0;
#if !defined(NDEBUG)
  // Some debug builds grow the stack too much.
  num_slots *= 2;
#endif
#if defined(ADDRESS_SANITIZER)
  // ASan bloats the stack variables.
  slot_size *= 2;
#endif
#if defined(LEAK_SANITIZER) && BUILDFLAG(IS_MAC)
  // The first time an LSAN disable is fired on a thread, the LSAN Mac runtime
  // initializes a 56k object on the stack.
  additional_space += 56 * 1024;
#endif
#if DCHECK_IS_ON()
  // The thread restrictions add four BooleanWithOptionalStacks (~2k each).
  additional_space += sizeof(BooleanWithOptionalStack) * 4;
#endif

  Thread a("StartWithStackSize");
  Thread::Options options;
  options.stack_size = num_slots * slot_size + additional_space;
  EXPECT_TRUE(a.StartWithOptions(std::move(options)));
  EXPECT_TRUE(a.task_runner());
  EXPECT_TRUE(a.IsRunning());

  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  a.task_runner()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event)));
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
  WaitableEvent block_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                            WaitableEvent::InitialState::NOT_SIGNALED);
  a->task_runner()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Wait, Unretained(&block_event)));

  a->StopSoon();
  EXPECT_TRUE(a->IsRunning());

  // Unblock the task and give a bit of extra time to unwind QuitWhenIdle().
  block_event.Signal();
  PlatformThread::Sleep(Milliseconds(20));

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
        FROM_HERE,
        BindOnce(static_cast<void (*)(TimeDelta)>(&PlatformThread::Sleep),
                 Milliseconds(20)));
    a.task_runner()->PostTask(FROM_HERE, BindOnce(&ToggleValue, &was_invoked));
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
  PlatformThread::Sleep(Milliseconds(20));
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
  EXPECT_DCHECK_DEATH_WITH(
      {
        // Stopping |a| on |b| isn't allowed.
        b.task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&Thread::Stop, Unretained(&a)));
        // Block here so the DCHECK on |b| always happens in this scope.
        PlatformThread::Sleep(TimeDelta::Max());
      },
      "owning_sequence_checker_.CalledOnValidSequence()");
}

TEST_F(ThreadTest, TransferOwnershipAndStop) {
  std::unique_ptr<Thread> a =
      std::make_unique<Thread>("TransferOwnershipAndStop");
  EXPECT_TRUE(a->StartAndWaitForTesting());
  EXPECT_TRUE(a->IsRunning());

  Thread b("TakingOwnershipThread");
  b.Start();

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  // a->DetachFromSequence() should allow |b| to use |a|'s Thread API.
  a->DetachFromSequence();
  b.task_runner()->PostTask(FROM_HERE,
                            BindOnce(
                                [](std::unique_ptr<Thread> thread_to_stop,
                                   WaitableEvent* event_to_signal) {
                                  thread_to_stop->Stop();
                                  event_to_signal->Signal();
                                },
                                std::move(a), Unretained(&event)));

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
  WaitableEvent last_task_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  a->task_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEvent::Signal,
                                                 Unretained(&last_task_event)));

  // StopSoon() is non-blocking, Yield() to |a|, wait for last task to be
  // processed and a little more for QuitWhenIdle() to unwind before considering
  // the thread "stopped".
  a->StopSoon();
  PlatformThread::YieldCurrentThread();
  last_task_event.Wait();
  PlatformThread::Sleep(Milliseconds(20));

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
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  PlatformThreadId id_from_new_thread;
  a.task_runner()->PostTask(
      FROM_HERE, BindOnce(ReturnThreadId, &a, &id_from_new_thread, &event));

  // Call GetThreadId() on the current thread before calling event.Wait() so
  // that this test can find a race issue with TSAN.
  PlatformThreadId id_from_current_thread = a.GetThreadId();

  // Check if GetThreadId() returns consistent value in both threads.
  event.Wait();
  EXPECT_EQ(id_from_current_thread, id_from_new_thread);

  // A started thread should have a valid ID.
  EXPECT_NE(kInvalidThreadId, a.GetThreadId());
  EXPECT_NE(kInvalidThreadId, b.GetThreadId());

  // Each thread should have a different thread ID.
  EXPECT_NE(a.GetThreadId(), b.GetThreadId());
}

TEST_F(ThreadTest, ThreadIdWithRestart) {
  Thread a("ThreadIdWithRestart");
  PlatformThreadId previous_id = kInvalidThreadId;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_TRUE(a.Start());
    PlatformThreadId current_id = a.GetThreadId();
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
    t.task_runner()->PostTask(FROM_HERE,
                              BindOnce(&RegisterDestructionObserver,
                                       Unretained(&loop_destruction_observer)));

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

  constexpr TimeDelta kSleepPerTestTask = Milliseconds(50);
  constexpr size_t kNumSleepTasks = 5;

  const TimeTicks ticks_before_post = TimeTicks::Now();

  for (size_t i = 0; i < kNumSleepTasks; ++i) {
    a.task_runner()->PostTask(
        FROM_HERE, BindOnce(&PlatformThread::Sleep, kSleepPerTestTask));
  }

  // All tasks should have executed, as reflected by the elapsed time.
  a.FlushForTesting();
  EXPECT_GE(TimeTicks::Now() - ticks_before_post,
            kNumSleepTasks * kSleepPerTestTask);

  a.Stop();

  // Flushing a stopped thread should be a no-op.
  a.FlushForTesting();
}

namespace {

using TaskQueue = sequence_manager::TaskQueue;

class SequenceManagerThreadDelegate : public Thread::Delegate {
 public:
  SequenceManagerThreadDelegate()
      : sequence_manager_(sequence_manager::CreateUnboundSequenceManager()),
        task_queue_(sequence_manager_->CreateTaskQueue(
            TaskQueue::Spec(sequence_manager::QueueName::DEFAULT_TQ))) {
    sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
  }

  SequenceManagerThreadDelegate(const SequenceManagerThreadDelegate&) = delete;
  SequenceManagerThreadDelegate& operator=(
      const SequenceManagerThreadDelegate&) = delete;

  ~SequenceManagerThreadDelegate() override = default;

  // Thread::Delegate:

  scoped_refptr<SingleThreadTaskRunner> GetDefaultTaskRunner() override {
    return task_queue_->task_runner();
  }

  void BindToCurrentThread() override {
    sequence_manager_->BindToMessagePump(
        MessagePump::Create(MessagePumpType::DEFAULT));
  }

 private:
  std::unique_ptr<sequence_manager::SequenceManager> sequence_manager_;
  TaskQueue::Handle task_queue_;
};

}  // namespace

TEST_F(ThreadTest, ProvidedThreadDelegate) {
  Thread thread("ThreadDelegate");
  Thread::Options options;
  options.delegate = std::make_unique<SequenceManagerThreadDelegate>();

  scoped_refptr<SingleThreadTaskRunner> task_runner =
      options.delegate->GetDefaultTaskRunner();
  thread.StartWithOptions(std::move(options));

  WaitableEvent event;
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  event.Wait();

  thread.Stop();
}

}  // namespace base
