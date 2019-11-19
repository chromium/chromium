// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher.h"

#include <math.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;

enum State {
  INITIALIZED,        // Created ThreadWatch object.
  ACTIVATED,          // Thread watching activated.
  SENT_PING,          // Sent ping message to watched thread.
  RECEIVED_PONG,      // Received Pong message.
  DEACTIVATED,        // Thread watching de-activated.
};

enum WaitState {
  UNINITIALIZED,
  STARTED_WAITING,    // Start waiting for state_ to change to expected_state.
  STOPPED_WAITING,    // Done with the waiting.
  ALL_DONE,           // Done with waiting for STOPPED_WAITING.
};

enum CheckResponseState {
  UNKNOWN,
  SUCCESSFUL,         // CheckResponse was successful.
  FAILED,             // CheckResponse has failed.
};

// This class helps to track and manipulate thread state during tests. This
// class also has utility method to simulate hanging of watched thread by making
// the watched thread wait for a very long time by posting a task on watched
// thread that keeps it busy. It also has an utility method to block running of
// tests until ThreadWatcher object's post-condition state changes to an
// expected state.
class CustomThreadWatcher : public ThreadWatcher {
 public:
  State thread_watcher_state_;
  // Wait state may be accessed from VeryLongMethod on another thread.
  base::Lock wait_state_lock_;
  base::ConditionVariable wait_state_changed_;
  WaitState wait_state_;
  CheckResponseState check_response_state_;
  uint64_t ping_sent_;
  uint64_t pong_received_;
  int32_t success_response_;
  int32_t failed_response_;
  base::TimeTicks saved_ping_time_;
  uint64_t saved_ping_sequence_number_;
  base::RepeatingClosure on_state_changed_;

  CustomThreadWatcher(const BrowserThread::ID thread_id,
                      const std::string thread_name,
                      const TimeDelta& sleep_time,
                      const TimeDelta& unresponsive_time)
      : ThreadWatcher(WatchingParams(thread_id,
                                     thread_name,
                                     sleep_time,
                                     unresponsive_time,
                                     ThreadWatcherList::kUnresponsiveCount,
                                     true)),
        thread_watcher_state_(INITIALIZED),
        wait_state_changed_(&wait_state_lock_),
        wait_state_(UNINITIALIZED),
        check_response_state_(UNKNOWN),
        ping_sent_(0),
        pong_received_(0),
        success_response_(0),
        failed_response_(0),
        saved_ping_time_(base::TimeTicks::Now()),
        saved_ping_sequence_number_(0) {}

  State UpdateState(State new_state) {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    State old_state = thread_watcher_state_;
    if (old_state != DEACTIVATED)
      thread_watcher_state_ = new_state;
    if (new_state == SENT_PING)
      ++ping_sent_;
    if (new_state == RECEIVED_PONG)
      ++pong_received_;
    saved_ping_time_ = ping_time();
    saved_ping_sequence_number_ = ping_sequence_number();
    OnStateChanged();
    return old_state;
  }

  void UpdateWaitState(WaitState new_state) {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    {
      base::AutoLock auto_lock(wait_state_lock_);
      wait_state_ = new_state;
    }
    wait_state_changed_.Broadcast();
    OnStateChanged();
  }

  void ActivateThreadWatching() override {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    State old_state = UpdateState(ACTIVATED);
    EXPECT_EQ(old_state, INITIALIZED);
    ThreadWatcher::ActivateThreadWatching();
  }

  void DeActivateThreadWatching() override {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    State old_state = UpdateState(DEACTIVATED);
    EXPECT_TRUE(old_state == ACTIVATED || old_state == SENT_PING ||
                old_state == RECEIVED_PONG);
    ThreadWatcher::DeActivateThreadWatching();
  }

  void PostPingMessage() override {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    State old_state = UpdateState(SENT_PING);
    EXPECT_TRUE(old_state == ACTIVATED || old_state == RECEIVED_PONG);
    ThreadWatcher::PostPingMessage();
  }

  void OnPongMessage(uint64_t ping_sequence_number) override {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    State old_state = UpdateState(RECEIVED_PONG);
    EXPECT_TRUE(old_state == SENT_PING || old_state == DEACTIVATED);
    ThreadWatcher::OnPongMessage(ping_sequence_number);
  }

  void OnCheckResponsiveness(uint64_t ping_sequence_number) override {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    ThreadWatcher::OnCheckResponsiveness(ping_sequence_number);
    if (responsive_) {
      ++success_response_;
      check_response_state_ = SUCCESSFUL;
    } else {
      ++failed_response_;
      check_response_state_ = FAILED;
    }
    OnStateChanged();
  }

  void WaitForWaitStateChange(TimeDelta wait_time, WaitState expected_state) {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    on_state_changed_ = base::BindRepeating(
        [](CustomThreadWatcher* watcher, base::RepeatingClosure quit_closure,
           WaitState expected_state) {
          DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
          // No need to acquire wait_state_lock_ since we're on the same thread
          // that modifies wait_state_.
          if (watcher->wait_state_ == expected_state)
            quit_closure.Run();
        },
        base::Unretained(this), quit_closure, expected_state);
    base::CancelableClosure timeout_closure(base::BindRepeating(
        [](base::RepeatingClosure quit_closure) {
          FAIL() << "WaitForWaitStateChange timed out";
          quit_closure.Run();
        },
        quit_closure));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_closure.callback(), wait_time);
    run_loop.Run();
    on_state_changed_.Reset();
  }

  // May be called on any thread other than the WatchDogThread.
  void BusyWaitForWaitStateChange(TimeDelta wait_time,
                                  WaitState expected_state) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    TimeTicks end_time = TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(wait_state_lock_);
      while (wait_state_ != expected_state && TimeTicks::Now() < end_time)
        wait_state_changed_.TimedWait(end_time - TimeTicks::Now());
    }
  }

  void VeryLongMethod(TimeDelta wait_time) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    // ThreadWatcher tasks should not be allowed to execute while we're waiting,
    // so hog the thread until the state changes.
    BusyWaitForWaitStateChange(wait_time, STOPPED_WAITING);
    WatchDogThread::PostTask(
        FROM_HERE, base::BindRepeating(&CustomThreadWatcher::UpdateWaitState,
                                       base::Unretained(this), ALL_DONE));
  }

  State WaitForStateChange(const TimeDelta& wait_time, State expected_state) {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    UpdateWaitState(STARTED_WAITING);

    // Keep the watch dog thread looping until the state changes to the
    // expected_state or until wait_time elapses enough times for the
    // unresponsive threshold to be reached.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    State exit_state = INITIALIZED;
    on_state_changed_ = base::BindRepeating(
        [](CustomThreadWatcher* watcher, base::RepeatingClosure quit_closure,
           State expected_state, State* exit_state) {
          *exit_state = watcher->thread_watcher_state_;
          if (watcher->thread_watcher_state_ == expected_state)
            quit_closure.Run();
        },
        base::Unretained(this), quit_closure, expected_state,
        base::Unretained(&exit_state));
    base::CancelableClosure timeout_closure(base::BindRepeating(
        [](base::RepeatingClosure quit_closure) { quit_closure.Run(); },
        quit_closure));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_closure.callback(),
        wait_time * unresponsive_threshold_);
    run_loop.Run();
    on_state_changed_.Reset();

    UpdateWaitState(STOPPED_WAITING);
    return exit_state;
  }

  CheckResponseState WaitForCheckResponse(const TimeDelta& wait_time,
                                          CheckResponseState expected_state) {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    UpdateWaitState(STARTED_WAITING);

    // Keep the watch dog thread looping until the state changes to the
    // expected_state or until wait_time elapses enough times for the
    // unresponsive threshold to be reached.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    CheckResponseState exit_state = UNKNOWN;
    on_state_changed_ = base::BindRepeating(
        [](CustomThreadWatcher* watcher, base::RepeatingClosure quit_closure,
           CheckResponseState expected_state, CheckResponseState* exit_state) {
          *exit_state = watcher->check_response_state_;
          if (watcher->check_response_state_ == expected_state)
            quit_closure.Run();
        },
        base::Unretained(this), quit_closure, expected_state,
        base::Unretained(&exit_state));
    base::CancelableClosure timeout_closure(base::BindRepeating(
        [](base::RepeatingClosure quit_closure) { quit_closure.Run(); },
        quit_closure));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_closure.callback(),
        wait_time * unresponsive_threshold_);
    run_loop.Run();
    on_state_changed_.Reset();

    UpdateWaitState(STOPPED_WAITING);
    return exit_state;
  }

  void OnStateChanged() {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    if (on_state_changed_)
      on_state_changed_.Run();
  }
};

class ThreadWatcherTest : public ::testing::Test {
 public:
  static constexpr TimeDelta kSleepTime = TimeDelta::FromMilliseconds(50);
  static constexpr TimeDelta kUnresponsiveTime =
      TimeDelta::FromMilliseconds(500);
  static constexpr char kIOThreadName[] = "IO";
  static constexpr char kUIThreadName[] = "UI";
  static constexpr char kCrashOnHangThreadNames[] = "UI,IO";
  static constexpr char kCrashOnHangThreadData[] = "UI:12,IO:12";

  CustomThreadWatcher* io_watcher_;
  CustomThreadWatcher* ui_watcher_;
  ThreadWatcherList* thread_watcher_list_;

  ThreadWatcherTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        setup_complete_(&lock_),
        initialized_(false) {
    // Make sure UI and IO threads are started and ready.
    task_environment_.RunIOThreadUntilIdle();

    watchdog_thread_.reset(new WatchDogThread());
    watchdog_thread_->StartAndWaitForTesting();

    WatchDogThread::PostTask(
        FROM_HERE, base::BindRepeating(&ThreadWatcherTest::SetUpObjects,
                                       base::Unretained(this)));

    WaitForSetUp(TimeDelta::FromMinutes(1));
  }

  void SetUpObjects() {
    DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

    // Setup the registry for thread watchers.
    thread_watcher_list_ = new ThreadWatcherList();

    // Create thread watcher object for the IO thread.
    std::unique_ptr<CustomThreadWatcher> io_watcher(new CustomThreadWatcher(
        BrowserThread::IO, kIOThreadName, kSleepTime, kUnresponsiveTime));
    io_watcher_ = io_watcher.get();
    ThreadWatcher* registered_io_watcher =
        ThreadWatcherList::Register(std::move(io_watcher));
    EXPECT_EQ(io_watcher_, registered_io_watcher);
    EXPECT_EQ(io_watcher_, thread_watcher_list_->Find(BrowserThread::IO));

    // Create thread watcher object for the UI thread.
    std::unique_ptr<CustomThreadWatcher> ui_watcher(new CustomThreadWatcher(
        BrowserThread::UI, kUIThreadName, kSleepTime, kUnresponsiveTime));
    ui_watcher_ = ui_watcher.get();
    ThreadWatcher* registered_ui_watcher =
        ThreadWatcherList::Register(std::move(ui_watcher));
    EXPECT_EQ(ui_watcher_, registered_ui_watcher);
    EXPECT_EQ(ui_watcher_, thread_watcher_list_->Find(BrowserThread::UI));

    {
      base::AutoLock lock(lock_);
      initialized_ = true;
    }
    setup_complete_.Signal();
  }

  void WaitForSetUp(TimeDelta wait_time) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    TimeTicks end_time = TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(lock_);
      while (!initialized_ && TimeTicks::Now() < end_time)
        setup_complete_.TimedWait(end_time - TimeTicks::Now());
    }
  }

  ~ThreadWatcherTest() override {
    ThreadWatcherList::DeleteAll();
    io_watcher_ = nullptr;
    ui_watcher_ = nullptr;
    watchdog_thread_.reset();
    thread_watcher_list_ = nullptr;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::Lock lock_;
  base::ConditionVariable setup_complete_;
  bool initialized_;
  std::unique_ptr<WatchDogThread> watchdog_thread_;

  DISALLOW_COPY_AND_ASSIGN(ThreadWatcherTest);
};

// Test fixture that runs a test body on the WatchDogThread. Subclasses override
// TestBodyOnWatchDogThread() and should call RunTestOnWatchDogThread() in their
// TEST_F() declaration.
class ThreadWatcherTestOnWatchDogThread : public ThreadWatcherTest {
 public:
  ThreadWatcherTestOnWatchDogThread()
      : test_body_run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {}

 protected:
  void RunTestOnWatchDogThread() {
    WatchDogThread::PostTask(FROM_HERE,
                             base::BindRepeating(
                                 [](ThreadWatcherTestOnWatchDogThread* test) {
                                   test->TestBodyOnWatchDogThread();
                                   test->test_body_run_loop_.Quit();
                                 },
                                 base::Unretained(this)));
    test_body_run_loop_.Run();
  }

  virtual void TestBodyOnWatchDogThread() = 0;

 protected:
  base::RunLoop test_body_run_loop_;
};

// Declare storage for ThreadWatcherTest's static constants.
constexpr TimeDelta ThreadWatcherTest::kSleepTime;
constexpr TimeDelta ThreadWatcherTest::kUnresponsiveTime;
constexpr char ThreadWatcherTest::kIOThreadName[];
constexpr char ThreadWatcherTest::kUIThreadName[];
constexpr char ThreadWatcherTest::kCrashOnHangThreadNames[];
constexpr char ThreadWatcherTest::kCrashOnHangThreadData[];

TEST_F(ThreadWatcherTest, ThreadNamesOnlyArgs) {
  // Setup command_line arguments.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kCrashOnHangThreads,
                                 kCrashOnHangThreadNames);

  // Parse command_line arguments.
  ThreadWatcherList::CrashOnHangThreadMap crash_on_hang_threads;
  uint32_t unresponsive_threshold;
  ThreadWatcherList::ParseCommandLine(command_line,
                                      &unresponsive_threshold,
                                      &crash_on_hang_threads);

  // Verify the data.
  base::CStringTokenizer tokens(
      kCrashOnHangThreadNames,
      kCrashOnHangThreadNames + (base::size(kCrashOnHangThreadNames) - 1), ",");
  while (tokens.GetNext()) {
    std::vector<base::StringPiece> values = base::SplitStringPiece(
        tokens.token_piece(), ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::string thread_name = values[0].as_string();

    auto it = crash_on_hang_threads.find(thread_name);
    bool crash_on_hang = (it != crash_on_hang_threads.end());
    EXPECT_TRUE(crash_on_hang);
    EXPECT_LT(0u, it->second);
  }
}

TEST_F(ThreadWatcherTest, CrashOnHangThreadsAllArgs) {
  // Setup command_line arguments.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kCrashOnHangThreads,
                                 kCrashOnHangThreadData);

  // Parse command_line arguments.
  ThreadWatcherList::CrashOnHangThreadMap crash_on_hang_threads;
  uint32_t unresponsive_threshold;
  ThreadWatcherList::ParseCommandLine(command_line,
                                      &unresponsive_threshold,
                                      &crash_on_hang_threads);

  // Verify the data.
  base::CStringTokenizer tokens(
      kCrashOnHangThreadData,
      kCrashOnHangThreadData + (base::size(kCrashOnHangThreadData) - 1), ",");
  while (tokens.GetNext()) {
    std::vector<base::StringPiece> values = base::SplitStringPiece(
        tokens.token_piece(), ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::string thread_name = values[0].as_string();

    auto it = crash_on_hang_threads.find(thread_name);

    bool crash_on_hang = (it != crash_on_hang_threads.end());
    EXPECT_TRUE(crash_on_hang);

    uint32_t crash_unresponsive_threshold = it->second;
    uint32_t crash_on_unresponsive_seconds =
        ThreadWatcherList::kUnresponsiveSeconds * crash_unresponsive_threshold;
    EXPECT_EQ(12u, crash_on_unresponsive_seconds);
  }
}

// Test registration. When thread_watcher_list_ goes out of scope after
// TearDown, all thread watcher objects will be deleted.
class ThreadWatcherTestRegistration : public ThreadWatcherTestOnWatchDogThread {
 protected:
  void TestBodyOnWatchDogThread() override {
    // Check ThreadWatcher object has all correct parameters.
    EXPECT_EQ(BrowserThread::IO, io_watcher_->thread_id());
    EXPECT_EQ(kIOThreadName, io_watcher_->thread_name());
    EXPECT_EQ(kSleepTime, io_watcher_->sleep_time());
    EXPECT_EQ(kUnresponsiveTime, io_watcher_->unresponsive_time());
    EXPECT_FALSE(io_watcher_->active());

    // Check ThreadWatcher object of watched UI thread has correct data.
    EXPECT_EQ(BrowserThread::UI, ui_watcher_->thread_id());
    EXPECT_EQ(kUIThreadName, ui_watcher_->thread_name());
    EXPECT_EQ(kSleepTime, ui_watcher_->sleep_time());
    EXPECT_EQ(kUnresponsiveTime, ui_watcher_->unresponsive_time());
    EXPECT_FALSE(ui_watcher_->active());
  }
};

TEST_F(ThreadWatcherTestRegistration, RunTest) {
  RunTestOnWatchDogThread();
}

// Test ActivateThreadWatching and DeActivateThreadWatching of IO thread. This
// method also checks that pong message was sent by the watched thread and pong
// message was received by the WatchDogThread. It also checks that
// OnCheckResponsiveness has verified the ping-pong mechanism and the watched
// thread is not hung.
class ThreadWatcherTestThreadResponding
    : public ThreadWatcherTestOnWatchDogThread {
 protected:
  void TestBodyOnWatchDogThread() override {
    TimeTicks time_before_ping = TimeTicks::Now();
    // Activate watching IO thread.
    io_watcher_->ActivateThreadWatching();

    // Activate would have started ping/pong messaging. Expect at least one
    // ping/pong messaging sequence to happen.
    io_watcher_->WaitForStateChange(kSleepTime + TimeDelta::FromMinutes(1),
                                    RECEIVED_PONG);
    EXPECT_GT(io_watcher_->ping_sent_, static_cast<uint64_t>(0));
    EXPECT_GT(io_watcher_->pong_received_, static_cast<uint64_t>(0));
    EXPECT_TRUE(io_watcher_->active());
    EXPECT_GE(io_watcher_->saved_ping_time_, time_before_ping);
    EXPECT_GE(io_watcher_->saved_ping_sequence_number_,
              static_cast<uint64_t>(0));

    // Verify watched thread is responding with ping/pong messaging.
    io_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
    EXPECT_GT(io_watcher_->success_response_, 0);
    EXPECT_EQ(io_watcher_->failed_response_, 0);

    // DeActivate thread watching for shutdown.
    io_watcher_->DeActivateThreadWatching();
  }
};

TEST_F(ThreadWatcherTestThreadResponding, RunTest) {
  RunTestOnWatchDogThread();
}

// This test posts a task on watched thread that takes very long time (this is
// to simulate hanging of watched thread). It then checks for
// OnCheckResponsiveness raising an alert (OnCheckResponsiveness returns false
// if the watched thread is not responding).
class ThreadWatcherTestThreadNotResponding
    : public ThreadWatcherTestOnWatchDogThread {
 protected:
  void TestBodyOnWatchDogThread() override {
    // Simulate hanging of watched thread by making the watched thread wait for
    // a very long time by posting a task on watched thread that keeps it busy.
    // It is safe to use base::Unretained because test is waiting for the method
    // to finish.
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CustomThreadWatcher::VeryLongMethod,
                       base::Unretained(io_watcher_), kUnresponsiveTime * 10));

    // Activate thread watching.
    io_watcher_->ActivateThreadWatching();

    // Verify watched thread is not responding for ping messages.
    io_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), FAILED);
    EXPECT_EQ(io_watcher_->success_response_, 0);
    EXPECT_GT(io_watcher_->failed_response_, 0);

    // DeActivate thread watching for shutdown.
    io_watcher_->DeActivateThreadWatching();

    // Wait for the io_watcher_'s VeryLongMethod to finish.
    io_watcher_->WaitForWaitStateChange(kUnresponsiveTime * 10, ALL_DONE);
  }
};

TEST_F(ThreadWatcherTestThreadNotResponding, RunTest) {
  RunTestOnWatchDogThread();
}

// Test watching of multiple threads with all threads not responding.
class ThreadWatcherTestMultipleThreadsResponding
    : public ThreadWatcherTestOnWatchDogThread {
 protected:
  void TestBodyOnWatchDogThread() override {
    // Check for UI thread to perform ping/pong messaging.
    ui_watcher_->ActivateThreadWatching();

    // Check for IO thread to perform ping/pong messaging.
    io_watcher_->ActivateThreadWatching();

    // Verify UI thread is responding with ping/pong messaging.
    ui_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
    EXPECT_GT(ui_watcher_->ping_sent_, static_cast<uint64_t>(0));
    EXPECT_GT(ui_watcher_->pong_received_, static_cast<uint64_t>(0));
    EXPECT_GE(ui_watcher_->ping_sequence_number(), static_cast<uint64_t>(0));
    EXPECT_GT(ui_watcher_->success_response_, 0);
    EXPECT_EQ(ui_watcher_->failed_response_, 0);

    // Verify IO thread is responding with ping/pong messaging.
    io_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
    EXPECT_GT(io_watcher_->ping_sent_, static_cast<uint64_t>(0));
    EXPECT_GT(io_watcher_->pong_received_, static_cast<uint64_t>(0));
    EXPECT_GE(io_watcher_->ping_sequence_number(), static_cast<uint64_t>(0));
    EXPECT_GT(io_watcher_->success_response_, 0);
    EXPECT_EQ(io_watcher_->failed_response_, 0);

    // DeActivate thread watching for shutdown.
    io_watcher_->DeActivateThreadWatching();
    ui_watcher_->DeActivateThreadWatching();
  }
};

TEST_F(ThreadWatcherTestMultipleThreadsResponding, RunTest) {
  RunTestOnWatchDogThread();
}

// Test watching of multiple threads with one of the threads not responding.
class ThreadWatcherTestMultipleThreadsNotResponding
    : public ThreadWatcherTestOnWatchDogThread {
 protected:
  void TestBodyOnWatchDogThread() override {
    // Simulate hanging of watched thread by making the watched thread wait for
    // a very long time by posting a task on watched thread that keeps it busy.
    // It is safe to use base::Unretained because test is waiting for the method
    // to finish.
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CustomThreadWatcher::VeryLongMethod,
                       base::Unretained(io_watcher_), kUnresponsiveTime * 10));

    // Activate watching of UI thread.
    ui_watcher_->ActivateThreadWatching();

    // Activate watching of IO thread.
    io_watcher_->ActivateThreadWatching();

    // Verify UI thread is responding with ping/pong messaging.
    ui_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
    EXPECT_GT(ui_watcher_->success_response_, 0);
    EXPECT_EQ(ui_watcher_->failed_response_, 0);

    // Verify IO thread is not responding for ping messages.
    io_watcher_->WaitForCheckResponse(
        kUnresponsiveTime + TimeDelta::FromMinutes(1), FAILED);
    EXPECT_EQ(io_watcher_->success_response_, 0);
    EXPECT_GT(io_watcher_->failed_response_, 0);

    // DeActivate thread watching for shutdown.
    io_watcher_->DeActivateThreadWatching();
    ui_watcher_->DeActivateThreadWatching();

    // Wait for the io_watcher_'s VeryLongMethod to finish.
    io_watcher_->WaitForWaitStateChange(kUnresponsiveTime * 10, ALL_DONE);
  }
};

TEST_F(ThreadWatcherTestMultipleThreadsNotResponding, RunTest) {
  RunTestOnWatchDogThread();
}

class ThreadWatcherListTest : public ::testing::Test {
 protected:
  ThreadWatcherListTest()
      : done_(&lock_),
        state_available_(false),
        has_thread_watcher_list_(false),
        stopped_(false) {}

  void ReadStateOnWatchDogThread() {
    CHECK(WatchDogThread::CurrentlyOnWatchDogThread());
    {
      base::AutoLock auto_lock(lock_);
      has_thread_watcher_list_ =
          ThreadWatcherList::g_thread_watcher_list_ != nullptr;
      stopped_ = ThreadWatcherList::g_stopped_;
      state_available_ = true;
    }
    done_.Signal();
  }

  void CheckState(bool has_thread_watcher_list,
                  bool stopped,
                  const char* const msg) {
    CHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    {
      base::AutoLock auto_lock(lock_);
      state_available_ = false;
    }

    WatchDogThread::PostTask(
        FROM_HERE,
        base::BindRepeating(&ThreadWatcherListTest::ReadStateOnWatchDogThread,
                            base::Unretained(this)));
    {
      base::AutoLock auto_lock(lock_);
      while (!state_available_)
        done_.Wait();

      EXPECT_EQ(has_thread_watcher_list, has_thread_watcher_list_) << msg;
      EXPECT_EQ(stopped, stopped_) << msg;
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  base::Lock lock_;
  base::ConditionVariable done_;

  bool state_available_;
  bool has_thread_watcher_list_;
  bool stopped_;
};

TEST_F(ThreadWatcherListTest, Restart) {
  ThreadWatcherList::g_initialize_delay_seconds = 1;

  std::unique_ptr<WatchDogThread> watchdog_thread_(new WatchDogThread());
  watchdog_thread_->StartAndWaitForTesting();

  // See http://crbug.com/347887.
  // StartWatchingAll() will PostDelayedTask to create g_thread_watcher_list_,
  // whilst StopWatchingAll() will just PostTask to destroy it.
  // Ensure that when Stop is called, Start will NOT create
  // g_thread_watcher_list_ later on.
  ThreadWatcherList::StartWatchingAll(*base::CommandLine::ForCurrentProcess());
  ThreadWatcherList::StopWatchingAll();
  {
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                          run_loop.QuitWhenIdleClosure(),
                          base::TimeDelta::FromSeconds(
                              ThreadWatcherList::g_initialize_delay_seconds));
    run_loop.Run();
  }

  CheckState(false /* has_thread_watcher_list */, true /* stopped */,
             "Start / Stopped");

  // Proceed with just |StartWatchingAll| and ensure it'll be started.
  ThreadWatcherList::StartWatchingAll(*base::CommandLine::ForCurrentProcess());
  {
    base::RunLoop run_loop;
    base::PostDelayedTask(
        FROM_HERE, {BrowserThread::UI}, run_loop.QuitWhenIdleClosure(),
        base::TimeDelta::FromSeconds(
            ThreadWatcherList::g_initialize_delay_seconds + 1));
    run_loop.Run();
  }

  CheckState(true /* has_thread_watcher_list */, false /* stopped */,
             "Started");

  // Finally, StopWatchingAll() must stop.
  ThreadWatcherList::StopWatchingAll();
  {
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                          run_loop.QuitWhenIdleClosure(),
                          base::TimeDelta::FromSeconds(
                              ThreadWatcherList::g_initialize_delay_seconds));
    run_loop.Run();
  }

  CheckState(false /* has_thread_watcher_list */, true /* stopped */,
             "Stopped");
}
