// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// The message loops on which each timer should be tested.
const MessageLoop::Type testing_message_loops[] = {
    MessageLoop::TYPE_DEFAULT, MessageLoop::TYPE_IO,
#if !defined(OS_IOS)  // iOS does not allow direct running of the UI loop.
    MessageLoop::TYPE_UI,
#endif
};

class Receiver {
 public:
  Receiver() : count_(0) {}
  void OnCalled() { count_++; }
  bool WasCalled() { return count_ > 0; }
  int TimesCalled() { return count_; }

 private:
  int count_;
};

// A basic helper class that can start a one-shot timer and signal a
// WaitableEvent when this timer fires.
class OneShotTimerTesterBase {
 public:
  // |did_run|, if provided, will be signaled when Run() fires.
  explicit OneShotTimerTesterBase(
      WaitableEvent* did_run = nullptr,
      const TimeDelta& delay = TimeDelta::FromMilliseconds(10))
      : did_run_(did_run), delay_(delay) {}

  virtual ~OneShotTimerTesterBase() = default;

  void Start() {
    started_time_ = TimeTicks::Now();
    timer_->Start(FROM_HERE, delay_, this, &OneShotTimerTesterBase::Run);
  }

  bool IsRunning() { return timer_->IsRunning(); }

  TimeTicks started_time() const { return started_time_; }
  TimeDelta delay() const { return delay_; }

 protected:
  virtual void Run() {
    if (did_run_) {
      EXPECT_FALSE(did_run_->IsSignaled());
      did_run_->Signal();
    }
  }

  std::unique_ptr<OneShotTimer> timer_ = std::make_unique<OneShotTimer>();

 private:
  WaitableEvent* const did_run_;
  const TimeDelta delay_;
  TimeTicks started_time_;

  DISALLOW_COPY_AND_ASSIGN(OneShotTimerTesterBase);
};

// Extends functionality of OneShotTimerTesterBase with the abilities to wait
// until the timer fires and to change task runner for the timer.
class OneShotTimerTester : public OneShotTimerTesterBase {
 public:
  // |did_run|, if provided, will be signaled when Run() fires.
  explicit OneShotTimerTester(
      WaitableEvent* did_run = nullptr,
      const TimeDelta& delay = TimeDelta::FromMilliseconds(10))
      : OneShotTimerTesterBase(did_run, delay),
        quit_closure_(run_loop_.QuitClosure()) {}

  ~OneShotTimerTester() override = default;

  void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
    timer_->SetTaskRunner(std::move(task_runner));

    // Run() will be invoked on |task_runner| but |run_loop_|'s QuitClosure
    // needs to run on this thread (where the MessageLoop lives).
    quit_closure_ = BindOnce(IgnoreResult(&SequencedTaskRunner::PostTask),
                             SequencedTaskRunnerHandle::Get(), FROM_HERE,
                             run_loop_.QuitClosure());
  }

  // Blocks until Run() executes and confirms that Run() didn't fire before
  // |delay_| expired.
  void WaitAndConfirmTimerFiredAfterDelay() {
    run_loop_.Run();

    EXPECT_NE(TimeTicks(), started_time());
    EXPECT_GE(TimeTicks::Now() - started_time(), delay());
  }

 protected:
  // Overridable method to do things on Run() before signaling events/closures
  // managed by this helper.
  virtual void OnRun() {}

 private:
  void Run() override {
    OnRun();
    OneShotTimerTesterBase::Run();
    std::move(quit_closure_).Run();
  }

  RunLoop run_loop_;
  OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(OneShotTimerTester);
};

class OneShotSelfDeletingTimerTester : public OneShotTimerTester {
 protected:
  void OnRun() override { timer_.reset(); }
};

constexpr int kNumRepeats = 10;

class RepeatingTimerTester {
 public:
  explicit RepeatingTimerTester(WaitableEvent* did_run, const TimeDelta& delay)
      : counter_(kNumRepeats),
        quit_closure_(run_loop_.QuitClosure()),
        did_run_(did_run),
        delay_(delay) {}

  void Start() {
    started_time_ = TimeTicks::Now();
    timer_.Start(FROM_HERE, delay_, this, &RepeatingTimerTester::Run);
  }

  void WaitAndConfirmTimerFiredRepeatedlyAfterDelay() {
    run_loop_.Run();

    EXPECT_NE(TimeTicks(), started_time_);
    EXPECT_GE(TimeTicks::Now() - started_time_, kNumRepeats * delay_);
  }

 private:
  void Run() {
    if (--counter_ == 0) {
      if (did_run_) {
        EXPECT_FALSE(did_run_->IsSignaled());
        did_run_->Signal();
      }
      timer_.Stop();
      quit_closure_.Run();
    }
  }

  RepeatingTimer timer_;
  int counter_;

  RunLoop run_loop_;
  Closure quit_closure_;
  WaitableEvent* const did_run_;

  const TimeDelta delay_;
  TimeTicks started_time_;

  DISALLOW_COPY_AND_ASSIGN(RepeatingTimerTester);
};

// Basic test with same setup as RunTest_OneShotTimers_Cancel below to confirm
// that |did_run_a| would be signaled in that test if it wasn't for the
// deletion.
void RunTest_OneShotTimers(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  WaitableEvent did_run_a(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  OneShotTimerTester a(&did_run_a);
  a.Start();

  OneShotTimerTester b;
  b.Start();

  b.WaitAndConfirmTimerFiredAfterDelay();

  EXPECT_TRUE(did_run_a.IsSignaled());
}

void RunTest_OneShotTimers_Cancel(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  WaitableEvent did_run_a(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  OneShotTimerTester* a = new OneShotTimerTester(&did_run_a);

  // This should run before the timer expires.
  SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  OneShotTimerTester b;
  b.Start();

  b.WaitAndConfirmTimerFiredAfterDelay();

  EXPECT_FALSE(did_run_a.IsSignaled());
}

void RunTest_OneShotSelfDeletingTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  OneShotSelfDeletingTimerTester f;
  f.Start();
  f.WaitAndConfirmTimerFiredAfterDelay();
}

void RunTest_RepeatingTimer(MessageLoop::Type message_loop_type,
                            const TimeDelta& delay) {
  MessageLoop loop(message_loop_type);

  RepeatingTimerTester f(nullptr, delay);
  f.Start();
  f.WaitAndConfirmTimerFiredRepeatedlyAfterDelay();
}

void RunTest_RepeatingTimer_Cancel(MessageLoop::Type message_loop_type,
                                   const TimeDelta& delay) {
  MessageLoop loop(message_loop_type);

  WaitableEvent did_run_a(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingTimerTester* a = new RepeatingTimerTester(&did_run_a, delay);

  // This should run before the timer expires.
  SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  RepeatingTimerTester b(nullptr, delay);
  b.Start();

  b.WaitAndConfirmTimerFiredRepeatedlyAfterDelay();

  // |a| should not have fired despite |b| starting after it on the same
  // sequence and being complete by now.
  EXPECT_FALSE(did_run_a.IsSignaled());
}

class DelayTimerTarget {
 public:
  bool signaled() const { return signaled_; }

  void Signal() {
    ASSERT_FALSE(signaled_);
    signaled_ = true;
  }

 private:
  bool signaled_ = false;
};

void RunTest_DelayTimer_NoCall(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // If Delay is never called, the timer shouldn't go off.
  DelayTimerTarget target;
  DelayTimer timer(FROM_HERE, TimeDelta::FromMilliseconds(1), &target,
                   &DelayTimerTarget::Signal);

  OneShotTimerTester tester;
  tester.Start();
  tester.WaitAndConfirmTimerFiredAfterDelay();

  ASSERT_FALSE(target.signaled());
}

void RunTest_DelayTimer_OneCall(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  DelayTimerTarget target;
  DelayTimer timer(FROM_HERE, TimeDelta::FromMilliseconds(1), &target,
                   &DelayTimerTarget::Signal);
  timer.Reset();

  OneShotTimerTester tester(nullptr, TimeDelta::FromMilliseconds(100));
  tester.Start();
  tester.WaitAndConfirmTimerFiredAfterDelay();

  ASSERT_TRUE(target.signaled());
}

struct ResetHelper {
  ResetHelper(DelayTimer* timer, DelayTimerTarget* target)
      : timer_(timer), target_(target) {}

  void Reset() {
    ASSERT_FALSE(target_->signaled());
    timer_->Reset();
  }

 private:
  DelayTimer* const timer_;
  DelayTimerTarget* const target_;
};

void RunTest_DelayTimer_Reset(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // If Delay is never called, the timer shouldn't go off.
  DelayTimerTarget target;
  DelayTimer timer(FROM_HERE, TimeDelta::FromMilliseconds(50), &target,
                   &DelayTimerTarget::Signal);
  timer.Reset();

  ResetHelper reset_helper(&timer, &target);

  OneShotTimer timers[20];
  for (size_t i = 0; i < arraysize(timers); ++i) {
    timers[i].Start(FROM_HERE, TimeDelta::FromMilliseconds(i * 10),
                    &reset_helper, &ResetHelper::Reset);
  }

  OneShotTimerTester tester(nullptr, TimeDelta::FromMilliseconds(300));
  tester.Start();
  tester.WaitAndConfirmTimerFiredAfterDelay();

  ASSERT_TRUE(target.signaled());
}

class DelayTimerFatalTarget {
 public:
  void Signal() {
    ASSERT_TRUE(false);
  }
};

void RunTest_DelayTimer_Deleted(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  DelayTimerFatalTarget target;

  {
    DelayTimer timer(FROM_HERE, TimeDelta::FromMilliseconds(50), &target,
                     &DelayTimerFatalTarget::Signal);
    timer.Reset();
  }

  // When the timer is deleted, the DelayTimerFatalTarget should never be
  // called.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that timers work properly in all configurations.

TEST(TimerTest, OneShotTimers) {
  for (auto i : testing_message_loops) {
    RunTest_OneShotTimers(i);
  }
}

TEST(TimerTest, OneShotTimers_Cancel) {
  for (auto i : testing_message_loops) {
    RunTest_OneShotTimers_Cancel(i);
  }
}

// If underline timer does not handle properly, we will crash or fail
// in full page heap environment.
TEST(TimerTest, OneShotSelfDeletingTimer) {
  for (auto i : testing_message_loops) {
    RunTest_OneShotSelfDeletingTimer(i);
  }
}

TEST(TimerTest, OneShotTimer_CustomTaskRunner) {
  // A MessageLoop is required for the timer events on the other thread to
  // communicate back to the Timer under test.
  MessageLoop loop;

  Thread other_thread("OneShotTimer_CustomTaskRunner");
  other_thread.Start();

  WaitableEvent did_run(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
  OneShotTimerTester f(&did_run);
  f.SetTaskRunner(other_thread.task_runner());
  f.Start();
  EXPECT_TRUE(f.IsRunning() || did_run.IsSignaled());

  f.WaitAndConfirmTimerFiredAfterDelay();
  EXPECT_TRUE(did_run.IsSignaled());

  // |f| should already have communicated back to this |loop| before invoking
  // Run() and as such this thread should already be aware that |f| is no longer
  // running.
  EXPECT_TRUE(loop.IsIdleForTesting());
  EXPECT_FALSE(f.IsRunning());
}

TEST(TimerTest, OneShotTimerWithTickClock) {
  scoped_refptr<TestMockTimeTaskRunner> task_runner(
      new TestMockTimeTaskRunner(Time::Now(), TimeTicks::Now()));
  MessageLoop message_loop;
  message_loop.SetTaskRunner(task_runner);
  Receiver receiver;
  OneShotTimer timer(task_runner->GetMockTickClock());
  timer.Start(FROM_HERE, TimeDelta::FromSeconds(1),
              BindOnce(&Receiver::OnCalled, Unretained(&receiver)));
  task_runner->FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_TRUE(receiver.WasCalled());
}

TEST(TimerTest, RepeatingTimer) {
  for (auto i : testing_message_loops) {
    RunTest_RepeatingTimer(i, TimeDelta::FromMilliseconds(10));
  }
}

TEST(TimerTest, RepeatingTimer_Cancel) {
  for (auto i : testing_message_loops) {
    RunTest_RepeatingTimer_Cancel(i, TimeDelta::FromMilliseconds(10));
  }
}

TEST(TimerTest, RepeatingTimerZeroDelay) {
  for (auto i : testing_message_loops) {
    RunTest_RepeatingTimer(i, TimeDelta::FromMilliseconds(0));
  }
}

TEST(TimerTest, RepeatingTimerZeroDelay_Cancel) {
  for (auto i : testing_message_loops) {
    RunTest_RepeatingTimer_Cancel(i, TimeDelta::FromMilliseconds(0));
  }
}

TEST(TimerTest, RepeatingTimerWithTickClock) {
  scoped_refptr<TestMockTimeTaskRunner> task_runner(
      new TestMockTimeTaskRunner(Time::Now(), TimeTicks::Now()));
  MessageLoop message_loop;
  message_loop.SetTaskRunner(task_runner);
  Receiver receiver;
  const int expected_times_called = 10;
  RepeatingTimer timer(task_runner->GetMockTickClock());
  timer.Start(FROM_HERE, TimeDelta::FromSeconds(1),
              BindRepeating(&Receiver::OnCalled, Unretained(&receiver)));
  task_runner->FastForwardBy(TimeDelta::FromSeconds(expected_times_called));
  timer.Stop();
  EXPECT_EQ(expected_times_called, receiver.TimesCalled());
}

TEST(TimerTest, DelayTimer_NoCall) {
  for (auto i : testing_message_loops) {
    RunTest_DelayTimer_NoCall(i);
  }
}

TEST(TimerTest, DelayTimer_OneCall) {
  for (auto i : testing_message_loops) {
    RunTest_DelayTimer_OneCall(i);
  }
}

// It's flaky on the buildbot, http://crbug.com/25038.
TEST(TimerTest, DISABLED_DelayTimer_Reset) {
  for (auto i : testing_message_loops) {
    RunTest_DelayTimer_Reset(i);
  }
}

TEST(TimerTest, DelayTimer_Deleted) {
  for (auto i : testing_message_loops) {
    RunTest_DelayTimer_Deleted(i);
  }
}

TEST(TimerTest, DelayTimerWithTickClock) {
  scoped_refptr<TestMockTimeTaskRunner> task_runner(
      new TestMockTimeTaskRunner(Time::Now(), TimeTicks::Now()));
  MessageLoop message_loop;
  message_loop.SetTaskRunner(task_runner);
  Receiver receiver;
  DelayTimer timer(FROM_HERE, TimeDelta::FromSeconds(1), &receiver,
                   &Receiver::OnCalled, task_runner->GetMockTickClock());
  task_runner->FastForwardBy(TimeDelta::FromMilliseconds(999));
  EXPECT_FALSE(receiver.WasCalled());
  timer.Reset();
  task_runner->FastForwardBy(TimeDelta::FromMilliseconds(999));
  EXPECT_FALSE(receiver.WasCalled());
  timer.Reset();
  task_runner->FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_TRUE(receiver.WasCalled());
}

TEST(TimerTest, MessageLoopShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking is enabled.
  WaitableEvent did_run(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
  {
    OneShotTimerTesterBase a(&did_run);
    OneShotTimerTesterBase b(&did_run);
    OneShotTimerTesterBase c(&did_run);
    OneShotTimerTesterBase d(&did_run);
    {
      MessageLoop loop;
      a.Start();
      b.Start();
    }  // MessageLoop destructs by falling out of scope.
  }  // OneShotTimers destruct.  SHOULD NOT CRASH, of course.

  EXPECT_FALSE(did_run.IsSignaled());
}

// Ref counted class which owns a Timer. The class passes a reference to itself
// via the |user_task| parameter in Timer::Start(). |Timer::user_task_| might
// end up holding the last reference to the class.
class OneShotSelfOwningTimerTester
    : public RefCounted<OneShotSelfOwningTimerTester> {
 public:
  OneShotSelfOwningTimerTester() = default;

  void StartTimer() {
    // Start timer with long delay in order to test the timer getting destroyed
    // while a timer task is still pending.
    timer_.Start(FROM_HERE, TimeDelta::FromDays(1),
                 BindOnce(&OneShotSelfOwningTimerTester::Run, this));
  }

 private:
  friend class RefCounted<OneShotSelfOwningTimerTester>;
  ~OneShotSelfOwningTimerTester() = default;

  void Run() {
    ADD_FAILURE() << "Timer unexpectedly fired.";
  }

  OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(OneShotSelfOwningTimerTester);
};

TEST(TimerTest, MessageLoopShutdownSelfOwningTimer) {
  // This test verifies that shutdown of the message loop does not cause crashes
  // if there is a pending timer not yet fired and |Timer::user_task_| owns the
  // timer. The test may only trigger exceptions if debug heap checking is
  // enabled.

  MessageLoop loop;
  scoped_refptr<OneShotSelfOwningTimerTester> tester =
      new OneShotSelfOwningTimerTester();

  std::move(tester)->StartTimer();
  // |Timer::user_task_| owns sole reference to |tester|.

  // MessageLoop destructs by falling out of scope. SHOULD NOT CRASH.
}

void TimerTestCallback() {
}

TEST(TimerTest, NonRepeatIsRunning) {
  {
    MessageLoop loop;
    OneShotTimer timer;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, TimeDelta::FromDays(1),
                BindOnce(&TimerTestCallback));
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
  }

  {
    RetainingOneShotTimer timer;
    MessageLoop loop;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, TimeDelta::FromDays(1),
                BindRepeating(&TimerTestCallback));
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
    ASSERT_FALSE(timer.user_task().is_null());
    timer.Reset();
    EXPECT_TRUE(timer.IsRunning());
  }
}

TEST(TimerTest, NonRepeatMessageLoopDeath) {
  OneShotTimer timer;
  {
    MessageLoop loop;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, TimeDelta::FromDays(1),
                BindOnce(&TimerTestCallback));
    EXPECT_TRUE(timer.IsRunning());
  }
  EXPECT_FALSE(timer.IsRunning());
}

TEST(TimerTest, RetainRepeatIsRunning) {
  MessageLoop loop;
  RepeatingTimer timer(FROM_HERE, TimeDelta::FromDays(1),
                       BindRepeating(&TimerTestCallback));
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

TEST(TimerTest, RetainNonRepeatIsRunning) {
  MessageLoop loop;
  RetainingOneShotTimer timer(FROM_HERE, TimeDelta::FromDays(1),
                              BindRepeating(&TimerTestCallback));
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

//-----------------------------------------------------------------------------

namespace {

bool g_callback_happened1 = false;
bool g_callback_happened2 = false;

void ClearAllCallbackHappened() {
  g_callback_happened1 = false;
  g_callback_happened2 = false;
}

void SetCallbackHappened1() {
  g_callback_happened1 = true;
  RunLoop::QuitCurrentWhenIdleDeprecated();
}

void SetCallbackHappened2() {
  g_callback_happened2 = true;
  RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

TEST(TimerTest, ContinuationStopStart) {
  {
    ClearAllCallbackHappened();
    MessageLoop loop;
    OneShotTimer timer;
    timer.Start(FROM_HERE, TimeDelta::FromMilliseconds(10),
                BindOnce(&SetCallbackHappened1));
    timer.Stop();
    timer.Start(FROM_HERE, TimeDelta::FromMilliseconds(40),
                BindOnce(&SetCallbackHappened2));
    RunLoop().Run();
    EXPECT_FALSE(g_callback_happened1);
    EXPECT_TRUE(g_callback_happened2);
  }
}

TEST(TimerTest, ContinuationReset) {
  {
    ClearAllCallbackHappened();
    MessageLoop loop;
    OneShotTimer timer;
    timer.Start(FROM_HERE, TimeDelta::FromMilliseconds(10),
                BindOnce(&SetCallbackHappened1));
    timer.Reset();
    // // Since Reset happened before task ran, the user_task must not be
    // cleared: ASSERT_FALSE(timer.user_task().is_null());
    RunLoop().Run();
    EXPECT_TRUE(g_callback_happened1);
  }
}

namespace {

// Fixture for tests requiring ScopedTaskEnvironment. Includes a WaitableEvent
// so that cases may Wait() on one thread and Signal() (explicitly, or
// implicitly via helper methods) on another.
class TimerSequenceTest : public testing::Test {
 public:
  TimerSequenceTest()
      : event_(WaitableEvent::ResetPolicy::AUTOMATIC,
               WaitableEvent::InitialState::NOT_SIGNALED) {}

  // Block until Signal() is called on another thread.
  void Wait() { event_.Wait(); }

  void Signal() { event_.Signal(); }

  // Helper to augment a task with a subsequent call to Signal().
  OnceClosure TaskWithSignal(OnceClosure task) {
    return BindOnce(&TimerSequenceTest::RunTaskAndSignal, Unretained(this),
                    std::move(task));
  }

  // Create the timer.
  void CreateTimer() { timer_.reset(new OneShotTimer); }

  // Schedule an event on the timer.
  void StartTimer(TimeDelta delay, OnceClosure task) {
    timer_->Start(FROM_HERE, delay, std::move(task));
  }

  void SetTaskRunnerForTimer(scoped_refptr<SequencedTaskRunner> task_runner) {
    timer_->SetTaskRunner(std::move(task_runner));
  }

  // Tell the timer to abandon the task.
  void AbandonTask() {
    EXPECT_TRUE(timer_->IsRunning());
    // Reset() to call Timer::AbandonScheduledTask()
    timer_->Reset();
    EXPECT_TRUE(timer_->IsRunning());
    timer_->Stop();
    EXPECT_FALSE(timer_->IsRunning());
  }

  static void VerifyAffinity(const SequencedTaskRunner* task_runner) {
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
  }

  // Delete the timer.
  void DeleteTimer() { timer_.reset(); }

 private:
  void RunTaskAndSignal(OnceClosure task) {
    std::move(task).Run();
    Signal();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  WaitableEvent event_;
  std::unique_ptr<OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(TimerSequenceTest);
};

}  // namespace

TEST_F(TimerSequenceTest, OneShotTimerTaskOnPoolSequence) {
  scoped_refptr<SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits({});

  base::RunLoop run_loop_;

  // Timer is created on this thread.
  CreateTimer();

  // Task will execute on a pool thread.
  SetTaskRunnerForTimer(task_runner);
  StartTimer(TimeDelta::FromMilliseconds(1),
             BindOnce(IgnoreResult(&SequencedTaskRunner::PostTask),
                      SequencedTaskRunnerHandle::Get(), FROM_HERE,
                      run_loop_.QuitClosure()));

  // Spin the loop so that the delayed task fires on it, which will forward it
  // to |task_runner|. And since the Timer's task is one that posts back to this
  // MessageLoop to quit, we finally unblock.
  run_loop_.Run();

  // Timer will be destroyed on this thread.
  DeleteTimer();
}

TEST_F(TimerSequenceTest, OneShotTimerUsedOnPoolSequence) {
  scoped_refptr<SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits({});

  // Timer is created on this thread.
  CreateTimer();

  // Task will be scheduled from a pool thread.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(&TimerSequenceTest::StartTimer, Unretained(this),
               TimeDelta::FromMilliseconds(1),
               BindOnce(&TimerSequenceTest::Signal, Unretained(this))));
  Wait();

  // Timer must be destroyed on pool thread, too.
  task_runner->PostTask(FROM_HERE,
                        TaskWithSignal(BindOnce(&TimerSequenceTest::DeleteTimer,
                                                Unretained(this))));
  Wait();
}

TEST_F(TimerSequenceTest, OneShotTimerTwoSequencesAbandonTask) {
  scoped_refptr<SequencedTaskRunner> task_runner1 =
      base::CreateSequencedTaskRunnerWithTraits({});
  scoped_refptr<SequencedTaskRunner> task_runner2 =
      base::CreateSequencedTaskRunnerWithTraits({});

  // Create timer on sequence #1.
  task_runner1->PostTask(
      FROM_HERE, TaskWithSignal(BindOnce(&TimerSequenceTest::CreateTimer,
                                         Unretained(this))));
  Wait();

  // And tell it to execute on a different sequence (#2).
  task_runner1->PostTask(
      FROM_HERE,
      TaskWithSignal(BindOnce(&TimerSequenceTest::SetTaskRunnerForTimer,
                              Unretained(this), task_runner2)));
  Wait();

  // Task will be scheduled from sequence #1.
  task_runner1->PostTask(
      FROM_HERE, BindOnce(&TimerSequenceTest::StartTimer, Unretained(this),
                          TimeDelta::FromHours(1), DoNothing().Once()));

  // Abandon task - must be called from scheduling sequence (#1).
  task_runner1->PostTask(
      FROM_HERE, TaskWithSignal(BindOnce(&TimerSequenceTest::AbandonTask,
                                         Unretained(this))));
  Wait();

  // Timer must be destroyed on the sequence it was scheduled from (#1).
  task_runner1->PostTask(
      FROM_HERE, TaskWithSignal(BindOnce(&TimerSequenceTest::DeleteTimer,
                                         Unretained(this))));
  Wait();
}

TEST_F(TimerSequenceTest, OneShotTimerUsedAndTaskedOnDifferentSequences) {
  scoped_refptr<SequencedTaskRunner> task_runner1 =
      base::CreateSequencedTaskRunnerWithTraits({});
  scoped_refptr<SequencedTaskRunner> task_runner2 =
      base::CreateSequencedTaskRunnerWithTraits({});

  // Create timer on sequence #1.
  task_runner1->PostTask(
      FROM_HERE, TaskWithSignal(BindOnce(&TimerSequenceTest::CreateTimer,
                                         Unretained(this))));
  Wait();

  // And tell it to execute on a different sequence (#2).
  task_runner1->PostTask(
      FROM_HERE,
      TaskWithSignal(BindOnce(&TimerSequenceTest::SetTaskRunnerForTimer,
                              Unretained(this), task_runner2)));
  Wait();

  // Task will be scheduled from sequence #1.
  task_runner1->PostTask(
      FROM_HERE,
      BindOnce(&TimerSequenceTest::StartTimer, Unretained(this),
               TimeDelta::FromMilliseconds(1),
               TaskWithSignal(BindOnce(&TimerSequenceTest::VerifyAffinity,
                                       Unretained(task_runner2.get())))));

  Wait();

  // Timer must be destroyed on the sequence it was scheduled from (#1).
  task_runner1->PostTask(
      FROM_HERE, TaskWithSignal(BindOnce(&TimerSequenceTest::DeleteTimer,
                                         Unretained(this))));
  Wait();
}

}  // namespace base
