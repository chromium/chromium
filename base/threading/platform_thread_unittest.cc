// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/threading/platform_thread.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/threading/threading_features.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/threading/platform_thread_internal_posix.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/threading/platform_thread_win.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include "base/mac/mac_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace base {

// Trivial tests that thread runs and doesn't crash on create, join, or detach -

namespace {

class TrivialThread : public PlatformThread::Delegate {
 public:
  TrivialThread() : run_event_(WaitableEvent::ResetPolicy::MANUAL,
                               WaitableEvent::InitialState::NOT_SIGNALED) {}

  TrivialThread(const TrivialThread&) = delete;
  TrivialThread& operator=(const TrivialThread&) = delete;

  void ThreadMain() override { run_event_.Signal(); }

  WaitableEvent& run_event() { return run_event_; }

 private:
  WaitableEvent run_event_;
};

}  // namespace

TEST(PlatformThreadTest, TrivialJoin) {
  TrivialThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.run_event().IsSignaled());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  PlatformThread::Join(handle);
  ASSERT_TRUE(thread.run_event().IsSignaled());
}

TEST(PlatformThreadTest, TrivialJoinTimesTen) {
  TrivialThread thread[10];
  PlatformThreadHandle handle[std::size(thread)];

  for (auto& n : thread)
    ASSERT_FALSE(n.run_event().IsSignaled());
  for (size_t n = 0; n < std::size(thread); n++)
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
  for (auto n : handle)
    PlatformThread::Join(n);
  for (auto& n : thread)
    ASSERT_TRUE(n.run_event().IsSignaled());
}

// The following detach tests are by nature racy. The run_event approximates the
// end and termination of the thread, but threads could persist shortly after
// the test completes.
TEST(PlatformThreadTest, TrivialDetach) {
  TrivialThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.run_event().IsSignaled());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  PlatformThread::Detach(handle);
  thread.run_event().Wait();
}

TEST(PlatformThreadTest, TrivialDetachTimesTen) {
  TrivialThread thread[10];
  PlatformThreadHandle handle[std::size(thread)];

  for (auto& n : thread)
    ASSERT_FALSE(n.run_event().IsSignaled());
  for (size_t n = 0; n < std::size(thread); n++) {
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
    PlatformThread::Detach(handle[n]);
  }
  for (auto& n : thread)
    n.run_event().Wait();
}

// Tests of basic thread functions ---------------------------------------------

namespace {

class FunctionTestThread : public PlatformThread::Delegate {
 public:
  FunctionTestThread()
      : thread_id_(kInvalidThreadId),
        termination_ready_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED),
        terminate_thread_(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED),
        done_(false) {}

  FunctionTestThread(const FunctionTestThread&) = delete;
  FunctionTestThread& operator=(const FunctionTestThread&) = delete;

  ~FunctionTestThread() override {
    EXPECT_TRUE(terminate_thread_.IsSignaled())
        << "Need to mark thread for termination and join the underlying thread "
        << "before destroying a FunctionTestThread as it owns the "
        << "WaitableEvent blocking the underlying thread's main.";
  }

  // Grabs |thread_id_|, runs an optional test on that thread, signals
  // |termination_ready_|, and then waits for |terminate_thread_| to be
  // signaled before exiting.
  void ThreadMain() override {
    thread_id_ = PlatformThread::CurrentId();
    EXPECT_NE(thread_id_, kInvalidThreadId);

    // Make sure that the thread ID is the same across calls.
    EXPECT_EQ(thread_id_, PlatformThread::CurrentId());

    // Run extra tests.
    RunTest();

    termination_ready_.Signal();
    terminate_thread_.Wait();

    done_ = true;
  }

  PlatformThreadId thread_id() const {
    EXPECT_TRUE(termination_ready_.IsSignaled()) << "Thread ID still unknown";
    return thread_id_;
  }

  bool IsRunning() const {
    return termination_ready_.IsSignaled() && !done_;
  }

  // Blocks until this thread is started and ready to be terminated.
  void WaitForTerminationReady() { termination_ready_.Wait(); }

  // Marks this thread for termination (callers must then join this thread to be
  // guaranteed of termination).
  void MarkForTermination() { terminate_thread_.Signal(); }

 private:
  // Runs an optional test on the newly created thread.
  virtual void RunTest() {}

  PlatformThreadId thread_id_;

  mutable WaitableEvent termination_ready_;
  WaitableEvent terminate_thread_;
  bool done_;
};

}  // namespace

TEST(PlatformThreadTest, Function) {
  PlatformThreadId main_thread_id = PlatformThread::CurrentId();

  FunctionTestThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.IsRunning());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  thread.WaitForTerminationReady();
  ASSERT_TRUE(thread.IsRunning());
  EXPECT_NE(thread.thread_id(), main_thread_id);

  thread.MarkForTermination();
  PlatformThread::Join(handle);
  ASSERT_FALSE(thread.IsRunning());

  // Make sure that the thread ID is the same across calls.
  EXPECT_EQ(main_thread_id, PlatformThread::CurrentId());
}

TEST(PlatformThreadTest, FunctionTimesTen) {
  PlatformThreadId main_thread_id = PlatformThread::CurrentId();

  FunctionTestThread thread[10];
  PlatformThreadHandle handle[std::size(thread)];

  for (const auto& n : thread)
    ASSERT_FALSE(n.IsRunning());

  for (size_t n = 0; n < std::size(thread); n++)
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
  for (auto& n : thread)
    n.WaitForTerminationReady();

  for (size_t n = 0; n < std::size(thread); n++) {
    ASSERT_TRUE(thread[n].IsRunning());
    EXPECT_NE(thread[n].thread_id(), main_thread_id);

    // Make sure no two threads get the same ID.
    for (size_t i = 0; i < n; ++i) {
      EXPECT_NE(thread[i].thread_id(), thread[n].thread_id());
    }
  }

  for (auto& n : thread)
    n.MarkForTermination();
  for (auto n : handle)
    PlatformThread::Join(n);
  for (const auto& n : thread)
    ASSERT_FALSE(n.IsRunning());

  // Make sure that the thread ID is the same across calls.
  EXPECT_EQ(main_thread_id, PlatformThread::CurrentId());
}

namespace {

constexpr ThreadType kAllThreadTypes[] = {
    ThreadType::kRealtimeAudio, ThreadType::kDisplayCritical,
    ThreadType::kDefault,       ThreadType::kResourceEfficient,
    ThreadType::kUtility,       ThreadType::kBackground};

class ThreadTypeTestThread : public FunctionTestThread {
 public:
  explicit ThreadTypeTestThread(ThreadType from, ThreadType to)
      : from_(from), to_(to) {}

  ThreadTypeTestThread(const ThreadTypeTestThread&) = delete;
  ThreadTypeTestThread& operator=(const ThreadTypeTestThread&) = delete;

  ~ThreadTypeTestThread() override = default;

 private:
  void RunTest() override {
    EXPECT_EQ(PlatformThread::GetCurrentThreadType(), ThreadType::kDefault);
    PlatformThread::SetCurrentThreadType(from_);
    EXPECT_EQ(PlatformThread::GetCurrentThreadType(), from_);
    PlatformThread::SetCurrentThreadType(to_);
    EXPECT_EQ(PlatformThread::GetCurrentThreadType(), to_);
  }

  const ThreadType from_;
  const ThreadType to_;
};

class ThreadPriorityTestThread : public FunctionTestThread {
 public:
  ThreadPriorityTestThread(ThreadType thread_type,
                           ThreadPriorityForTest priority)
      : thread_type_(thread_type), priority(priority) {}

 private:
  void RunTest() override {
    testing::Message message;
    message << "thread_type: " << static_cast<int>(thread_type_);
    SCOPED_TRACE(message);

    EXPECT_EQ(PlatformThread::GetCurrentThreadType(), ThreadType::kDefault);
    PlatformThread::SetCurrentThreadType(thread_type_);
    EXPECT_EQ(PlatformThread::GetCurrentThreadType(), thread_type_);
    if (PlatformThread::CanChangeThreadType(ThreadType::kDefault,
                                            thread_type_)) {
      EXPECT_EQ(PlatformThread::GetCurrentThreadPriorityForTest(), priority);
    }
  }

  const ThreadType thread_type_;
  const ThreadPriorityForTest priority;
};

void TestSetCurrentThreadType() {
  for (auto from : kAllThreadTypes) {
    if (!PlatformThread::CanChangeThreadType(ThreadType::kDefault, from)) {
      continue;
    }
    for (auto to : kAllThreadTypes) {
      ThreadTypeTestThread thread(from, to);
      PlatformThreadHandle handle;

      ASSERT_FALSE(thread.IsRunning());
      ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
      thread.WaitForTerminationReady();
      ASSERT_TRUE(thread.IsRunning());

      thread.MarkForTermination();
      PlatformThread::Join(handle);
      ASSERT_FALSE(thread.IsRunning());
    }
  }
}

void TestPriorityResultingFromThreadType(ThreadType thread_type,
                                         ThreadPriorityForTest priority) {
  ThreadPriorityTestThread thread(thread_type, priority);
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.IsRunning());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  thread.WaitForTerminationReady();
  ASSERT_TRUE(thread.IsRunning());

  thread.MarkForTermination();
  PlatformThread::Join(handle);
  ASSERT_FALSE(thread.IsRunning());
}

}  // namespace

// Test changing a created thread's type.
TEST(PlatformThreadTest, SetCurrentThreadType) {
  TestSetCurrentThreadType();
}

#if BUILDFLAG(IS_WIN)
// Test changing a created thread's priority in an IDLE_PRIORITY_CLASS process
// (regression test for https://crbug.com/901483).
TEST(PlatformThreadTest,
     SetCurrentThreadTypeWithThreadModeBackgroundIdleProcess) {
  ::SetPriorityClass(Process::Current().Handle(), IDLE_PRIORITY_CLASS);
  TestSetCurrentThreadType();
  ::SetPriorityClass(Process::Current().Handle(), NORMAL_PRIORITY_CLASS);
}
#endif  // BUILDFLAG(IS_WIN)

// Ideally PlatformThread::CanChangeThreadType() would be true on all
// platforms for all priorities. This not being the case. This test documents
// and hardcodes what we know. Please inform scheduler-dev@chromium.org if this
// proprerty changes for a given platform.
TEST(PlatformThreadTest, CanChangeThreadType) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Ubuntu, RLIMIT_NICE and RLIMIT_RTPRIO are 0 by default, so we won't be
  // able to increase priority to any level unless we are root (euid == 0).
  bool kCanIncreasePriority = false;
  if (geteuid() == 0) {
    kCanIncreasePriority = true;
  }

#else
  constexpr bool kCanIncreasePriority = true;
#endif

  for (auto type : kAllThreadTypes) {
    EXPECT_TRUE(PlatformThread::CanChangeThreadType(type, type));
  }
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                   ThreadType::kUtility));
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(
      ThreadType::kBackground, ThreadType::kResourceEfficient));
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                   ThreadType::kDefault));
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(ThreadType::kDefault,
                                                   ThreadType::kBackground));
#else
  EXPECT_EQ(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                ThreadType::kUtility),
            kCanIncreasePriority);
  EXPECT_EQ(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                ThreadType::kResourceEfficient),
            kCanIncreasePriority);
  EXPECT_EQ(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                ThreadType::kDefault),
            kCanIncreasePriority);
  EXPECT_TRUE(PlatformThread::CanChangeThreadType(ThreadType::kDefault,
                                                  ThreadType::kBackground));
#endif
  EXPECT_EQ(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                ThreadType::kDisplayCritical),
            kCanIncreasePriority);
  EXPECT_EQ(PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                                ThreadType::kRealtimeAudio),
            kCanIncreasePriority);
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(ThreadType::kDisplayCritical,
                                                   ThreadType::kBackground));
  EXPECT_FALSE(PlatformThread::CanChangeThreadType(ThreadType::kRealtimeAudio,
                                                   ThreadType::kBackground));
#else
  EXPECT_TRUE(PlatformThread::CanChangeThreadType(ThreadType::kDisplayCritical,
                                                  ThreadType::kBackground));
  EXPECT_TRUE(PlatformThread::CanChangeThreadType(ThreadType::kRealtimeAudio,
                                                  ThreadType::kBackground));
#endif
}

TEST(PlatformThreadTest, SetCurrentThreadTypeTest) {
  TestPriorityResultingFromThreadType(ThreadType::kBackground,
                                      ThreadPriorityForTest::kBackground);
  TestPriorityResultingFromThreadType(ThreadType::kUtility,
                                      ThreadPriorityForTest::kUtility);

#if BUILDFLAG(IS_APPLE)
  TestPriorityResultingFromThreadType(ThreadType::kResourceEfficient,
                                      ThreadPriorityForTest::kUtility);
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  TestPriorityResultingFromThreadType(
      ThreadType::kResourceEfficient,
      ThreadPriorityForTest::kResourceEfficient);
#else
  TestPriorityResultingFromThreadType(ThreadType::kResourceEfficient,
                                      ThreadPriorityForTest::kNormal);
#endif  // BUILDFLAG(IS_APPLE)

  TestPriorityResultingFromThreadType(ThreadType::kDefault,
                                      ThreadPriorityForTest::kNormal);
  TestPriorityResultingFromThreadType(ThreadType::kDisplayCritical,
                                      ThreadPriorityForTest::kDisplay);
  TestPriorityResultingFromThreadType(ThreadType::kRealtimeAudio,
                                      ThreadPriorityForTest::kRealtimeAudio);
}

TEST(PlatformThreadTest, SetHugeThreadName) {
  // Construct an excessively long thread name.
  std::string long_name(1024, 'a');

  // SetName has no return code, so just verify that implementations
  // don't [D]CHECK().
  PlatformThread::SetName(long_name);
}

TEST(PlatformThreadTest, GetDefaultThreadStackSize) {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();
#if BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK)
  EXPECT_EQ(1024u * 1024u, stack_size);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA) ||      \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__) && \
     !defined(THREAD_SANITIZER)) ||                                           \
    (BUILDFLAG(IS_ANDROID) && !defined(ADDRESS_SANITIZER))
  EXPECT_EQ(0u, stack_size);
#else
  EXPECT_GT(stack_size, 0u);
  EXPECT_LT(stack_size, 20u * (1 << 20));
#endif
}

#if BUILDFLAG(IS_APPLE)

namespace {

class RealtimeTestThread : public FunctionTestThread {
 public:
  explicit RealtimeTestThread(TimeDelta realtime_period)
      : realtime_period_(realtime_period) {}
  ~RealtimeTestThread() override = default;

 private:
  RealtimeTestThread(const RealtimeTestThread&) = delete;
  RealtimeTestThread& operator=(const RealtimeTestThread&) = delete;

  TimeDelta GetRealtimePeriod() final { return realtime_period_; }

  // Verifies the realtime thead configuration.
  void RunTest() override {
    EXPECT_EQ(PlatformThread::GetCurrentThreadType(),
              ThreadType::kRealtimeAudio);

    mach_port_t mach_thread_id = pthread_mach_thread_np(
        PlatformThread::CurrentHandle().platform_handle());

    // |count| and |get_default| chosen impirically so that
    // time_constraints_buffer[0] would store the last constraints that were
    // applied.
    const int kPolicyCount = 32;
    thread_time_constraint_policy_data_t time_constraints_buffer[kPolicyCount];
    mach_msg_type_number_t count = kPolicyCount;
    boolean_t get_default = 0;

    kern_return_t result = thread_policy_get(
        mach_thread_id, THREAD_TIME_CONSTRAINT_POLICY,
        reinterpret_cast<thread_policy_t>(time_constraints_buffer), &count,
        &get_default);

    EXPECT_EQ(result, KERN_SUCCESS);

    const thread_time_constraint_policy_data_t& time_constraints =
        time_constraints_buffer[0];

    mach_timebase_info_data_t tb_info;
    mach_timebase_info(&tb_info);

    if (FeatureList::IsEnabled(kOptimizedRealtimeThreadingMac) &&
        !realtime_period_.is_zero()) {
      uint32_t abs_realtime_period = saturated_cast<uint32_t>(
          realtime_period_.InNanoseconds() *
          (static_cast<double>(tb_info.denom) / tb_info.numer));

      EXPECT_EQ(time_constraints.period, abs_realtime_period);
      EXPECT_EQ(
          time_constraints.computation,
          static_cast<uint32_t>(abs_realtime_period *
                                kOptimizedRealtimeThreadingMacBusy.Get()));
      EXPECT_EQ(
          time_constraints.constraint,
          static_cast<uint32_t>(abs_realtime_period *
                                kOptimizedRealtimeThreadingMacBusyLimit.Get()));
      EXPECT_EQ(time_constraints.preemptible,
                kOptimizedRealtimeThreadingMacPreemptible.Get());
    } else {
      // Old-style empirical values.
      const double kTimeQuantum = 2.9;
      const double kAudioTimeNeeded = 0.75 * kTimeQuantum;
      const double kMaxTimeAllowed = 0.85 * kTimeQuantum;

      // Get the conversion factor from milliseconds to absolute time
      // which is what the time-constraints returns.
      double ms_to_abs_time = double(tb_info.denom) / tb_info.numer * 1000000;

      EXPECT_EQ(time_constraints.period,
                saturated_cast<uint32_t>(kTimeQuantum * ms_to_abs_time));
      EXPECT_EQ(time_constraints.computation,
                saturated_cast<uint32_t>(kAudioTimeNeeded * ms_to_abs_time));
      EXPECT_EQ(time_constraints.constraint,
                saturated_cast<uint32_t>(kMaxTimeAllowed * ms_to_abs_time));
      EXPECT_FALSE(time_constraints.preemptible);
    }
  }

  const TimeDelta realtime_period_;
};

class RealtimePlatformThreadTest
    : public testing::TestWithParam<
          std::tuple<bool, FieldTrialParams, TimeDelta>> {
 protected:
  void VerifyRealtimeConfig(TimeDelta period) {
    RealtimeTestThread thread(period);
    PlatformThreadHandle handle;

    ASSERT_FALSE(thread.IsRunning());
    ASSERT_TRUE(PlatformThread::CreateWithType(0, &thread, &handle,
                                               ThreadType::kRealtimeAudio));
    thread.WaitForTerminationReady();
    ASSERT_TRUE(thread.IsRunning());

    thread.MarkForTermination();
    PlatformThread::Join(handle);
    ASSERT_FALSE(thread.IsRunning());
  }
};

TEST_P(RealtimePlatformThreadTest, RealtimeAudioConfigMac) {
  test::ScopedFeatureList feature_list;
  if (std::get<0>(GetParam())) {
    feature_list.InitAndEnableFeatureWithParameters(
        kOptimizedRealtimeThreadingMac, std::get<1>(GetParam()));
  } else {
    feature_list.InitAndDisableFeature(kOptimizedRealtimeThreadingMac);
  }

  PlatformThread::InitializeFeatures();
  VerifyRealtimeConfig(std::get<2>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    RealtimePlatformThreadTest,
    RealtimePlatformThreadTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            FieldTrialParams{
                {kOptimizedRealtimeThreadingMacPreemptible.name, "true"}},
            FieldTrialParams{
                {kOptimizedRealtimeThreadingMacPreemptible.name, "false"}},
            FieldTrialParams{
                {kOptimizedRealtimeThreadingMacBusy.name, "0.5"},
                {kOptimizedRealtimeThreadingMacBusyLimit.name, "0.75"}},
            FieldTrialParams{
                {kOptimizedRealtimeThreadingMacBusy.name, "0.7"},
                {kOptimizedRealtimeThreadingMacBusyLimit.name, "0.7"}},
            FieldTrialParams{
                {kOptimizedRealtimeThreadingMacBusy.name, "0.5"},
                {kOptimizedRealtimeThreadingMacBusyLimit.name, "1.0"}}),
        testing::Values(TimeDelta(),
                        Seconds(256.0 / 48000),
                        Milliseconds(5),
                        Milliseconds(10),
                        Seconds(1024.0 / 44100),
                        Seconds(1024.0 / 16000))));

}  // namespace

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {

bool IsTidCacheCorrect() {
  return PlatformThread::CurrentId() == syscall(__NR_gettid);
}

void* CheckTidCacheCorrectWrapper(void*) {
  CHECK(IsTidCacheCorrect());
  return nullptr;
}

void CreatePthreadToCheckCache() {
  pthread_t thread_id;
  pthread_create(&thread_id, nullptr, CheckTidCacheCorrectWrapper, nullptr);
  pthread_join(thread_id, nullptr);
}

// This test must use raw pthreads and fork() to avoid calls from //base to
// PlatformThread::CurrentId(), as the ordering of calls is important to the
// test.
void TestTidCacheCorrect(bool main_thread_accesses_cache_first) {
  EXPECT_TRUE(IsTidCacheCorrect());

  CreatePthreadToCheckCache();

  // Now fork a process and make sure the TID cache gets correctly updated on
  // both its main thread and a child thread.
  pid_t child_pid = fork();
  ASSERT_GE(child_pid, 0);

  if (child_pid == 0) {
    // In the child.
    if (main_thread_accesses_cache_first) {
      if (!IsTidCacheCorrect())
        _exit(1);
    }

    // Access the TID cache on another thread and make sure the cached value is
    // correct.
    CreatePthreadToCheckCache();

    if (!main_thread_accesses_cache_first) {
      // Make sure the main thread's cache is correct even though another thread
      // accessed the cache first.
      if (!IsTidCacheCorrect())
        _exit(1);
    }

    _exit(0);
  }

  int status;
  ASSERT_EQ(waitpid(child_pid, &status, 0), child_pid);
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 0);
}

TEST(PlatformThreadTidCacheTest, MainThreadFirst) {
  TestTidCacheCorrect(true);
}

TEST(PlatformThreadTidCacheTest, MainThreadSecond) {
  TestTidCacheCorrect(false);
}

}  // namespace

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace base
