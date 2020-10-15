// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_affinity_posix.h"

#include <sched.h>

#include <string>

#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestThread : public PlatformThread::Delegate {
 public:
  TestThread()
      : termination_ready_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED),
        terminate_thread_(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED) {}
  TestThread(const TestThread&) = delete;
  TestThread& operator=(const TestThread&) = delete;
  ~TestThread() override {
    EXPECT_TRUE(terminate_thread_.IsSignaled())
        << "Need to mark thread for termination and join the underlying thread "
        << "before destroying a FunctionTestThread as it owns the "
        << "WaitableEvent blocking the underlying thread's main.";
  }

  // Grabs |thread_id_|, signals |termination_ready_|, and then waits for
  // |terminate_thread_| to be signaled before exiting.
  void ThreadMain() override {
    thread_id_ = PlatformThread::CurrentId();
    EXPECT_NE(thread_id_, kInvalidThreadId);

    // Make sure that the thread ID is the same across calls.
    EXPECT_EQ(thread_id_, PlatformThread::CurrentId());

    termination_ready_.Signal();
    terminate_thread_.Wait();

    done_ = true;
  }

  PlatformThreadId thread_id() const {
    EXPECT_TRUE(termination_ready_.IsSignaled()) << "Thread ID still unknown";
    return thread_id_;
  }

  bool IsRunning() const { return termination_ready_.IsSignaled() && !done_; }

  // Blocks until this thread is started and ready to be terminated.
  void WaitForTerminationReady() { termination_ready_.Wait(); }

  // Marks this thread for termination (callers must then join this thread to be
  // guaranteed of termination).
  void MarkForTermination() { terminate_thread_.Signal(); }

 private:
  PlatformThreadId thread_id_ = kInvalidThreadId;

  mutable WaitableEvent termination_ready_;
  WaitableEvent terminate_thread_;
  bool done_ = false;
};

}  // namespace

#if defined(OS_ANDROID)
#define MAYBE_SetThreadCpuAffinityMode SetThreadCpuAffinityMode
#else
// The test only considers Android device hardware models at the moment. Some
// CrOS devices on the waterfall have asymmetric CPUs that aren't covered. The
// linux-trusty-rel bot also fails to sched_setaffinity().
#define MAYBE_SetThreadCpuAffinityMode DISABLED_SetThreadCpuAffinityMode
#endif
TEST(CpuAffinityTest, MAYBE_SetThreadCpuAffinityMode) {
  // This test currently only supports Nexus 5x and Pixel devices as big.LITTLE
  // devices. For other devices, we assume that the cores are symmetric. This is
  // currently the case for the devices on our waterfalls.
  std::string device_model = SysInfo::HardwareModelName();
  int expected_total_cores = SysInfo::SysInfo::NumberOfProcessors();
  int expected_little_cores = expected_total_cores;
  if (device_model == "Nexus 5X" || device_model == "Pixel 2" ||
      device_model == "Pixel 2 XL" || device_model == "Pixel 3" ||
      device_model == "Pixel 3 XL" || device_model == "Pixel 4" ||
      device_model == "Pixel 4 XL") {
    expected_little_cores = 4;
    EXPECT_LT(expected_little_cores, expected_total_cores);
  } else if (device_model == "Pixel" || device_model == "Pixel XL") {
    expected_little_cores = 2;
    EXPECT_LT(expected_little_cores, expected_total_cores);
  } else if (device_model == "Pixel 3a" || device_model == "Pixel 3a XL") {
    expected_little_cores = 6;
    EXPECT_LT(expected_little_cores, expected_total_cores);
  } else if (device_model == "Nexus 5" || device_model == "Nexus 7") {
    // On our Nexus 5 and Nexus 7 bots, something else in the system seems to
    // set affinity for the test process, making these tests flaky
    // (crbug.com/1113964).
    return;
  }

  TestThread thread;
  PlatformThreadHandle handle;
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  thread.WaitForTerminationReady();
  ASSERT_TRUE(thread.IsRunning());

  PlatformThreadId thread_id = thread.thread_id();
  cpu_set_t set;

  EXPECT_TRUE(
      SetThreadCpuAffinityMode(thread_id, CpuAffinityMode::kLittleCoresOnly));
  EXPECT_EQ(sched_getaffinity(thread_id, sizeof(set), &set), 0);

  EXPECT_EQ(CPU_COUNT(&set), expected_little_cores);

  EXPECT_TRUE(SetThreadCpuAffinityMode(thread_id, CpuAffinityMode::kDefault));
  EXPECT_EQ(sched_getaffinity(thread_id, sizeof(set), &set), 0);

  EXPECT_EQ(CPU_COUNT(&set), expected_total_cores);

  thread.MarkForTermination();
  PlatformThread::Join(handle);
  ASSERT_FALSE(thread.IsRunning());
}

}  // namespace base
