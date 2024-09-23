// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_win.h"

#include <windows.h>

#include <array>

#include "base/process/process.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread_win.h"
#include "base/threading/simple_thread.h"
#include "base/threading/threading_features.h"
#include "base/win/windows_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// It has been observed that calling
// :SetThreadPriority(THREAD_MODE_BACKGROUND_BEGIN) in an IDLE_PRIORITY_CLASS
// process never affects the return value of ::GetThreadPriority() or
// the base priority reported in Process Explorer. It does however
// set the memory and I/O priorities to very low. This test confirms that
// behavior which we suspect is a Windows kernel bug. If this test starts
// failing, the mitigation for https://crbug.com/901483 in
// PlatformThread::SetCurrentThreadType() should be revisited.
TEST(PlatformThreadWinTest, SetBackgroundThreadModeFailsInIdlePriorityProcess) {
  PlatformThreadHandle::Handle thread_handle =
      PlatformThread::CurrentHandle().platform_handle();

  // ::GetThreadPriority() is NORMAL. Memory priority is NORMAL.
  // Note: There is no practical way to verify the I/O priority.
  EXPECT_EQ(::GetThreadPriority(thread_handle), THREAD_PRIORITY_NORMAL);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);

  // Set the process priority to IDLE.
  // Note: Do not use Process::SetPriority() because it uses
  // PROCESS_MODE_BACKGROUND_BEGIN instead of IDLE_PRIORITY_CLASS when
  // the target is the current process.
  EXPECT_EQ(::GetPriorityClass(Process::Current().Handle()),
            static_cast<DWORD>(NORMAL_PRIORITY_CLASS));
  ::SetPriorityClass(Process::Current().Handle(), IDLE_PRIORITY_CLASS);
  EXPECT_EQ(Process::Current().GetOSPriority(),
            static_cast<int>(IDLE_PRIORITY_CLASS));

  // GetThreadPriority() stays NORMAL. Memory priority stays NORMAL.
  EXPECT_EQ(::GetThreadPriority(thread_handle), THREAD_PRIORITY_NORMAL);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);

  // Begin thread mode background.
  EXPECT_TRUE(::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_BEGIN));

  // On Win10+, GetThreadPriority() stays NORMAL and memory priority becomes
  // VERY_LOW.
  //
  // Note: this documents the aforementioned kernel bug. Ideally this would
  // *not* be the case.
  const int priority_after_thread_mode_background_begin =
      ::GetThreadPriority(thread_handle);
  EXPECT_EQ(priority_after_thread_mode_background_begin,
            THREAD_PRIORITY_NORMAL);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_VERY_LOW);

  PlatformThread::Sleep(base::Seconds(1));

  // After 1 second, GetThreadPriority() and memory priority don't change (this
  // refutes the hypothesis that it simply takes time before GetThreadPriority()
  // is updated after entering thread mode background).
  EXPECT_EQ(::GetThreadPriority(thread_handle),
            priority_after_thread_mode_background_begin);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_VERY_LOW);

  // Set the process priority to NORMAL.
  ::SetPriorityClass(Process::Current().Handle(), NORMAL_PRIORITY_CLASS);

  // GetThreadPriority() and memory priority don't change when the process
  // priority changes.
  EXPECT_EQ(::GetThreadPriority(thread_handle),
            priority_after_thread_mode_background_begin);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_VERY_LOW);

  // End thread mode background.
  //
  // Note: at least "ending" the semi-enforced background mode works...
  EXPECT_TRUE(::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_END));

  // GetThreadPriority() stays/becomes NORMAL. Memory priority becomes NORMAL.
  EXPECT_EQ(::GetThreadPriority(thread_handle), THREAD_PRIORITY_NORMAL);
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);
}

namespace {
class MemoryPriorityAssertingThreadDelegate
    : public base::PlatformThread::Delegate {
 public:
  explicit MemoryPriorityAssertingThreadDelegate(LONG memory_priority)
      : memory_priority_(memory_priority) {}

  void ThreadMain() override {
    PlatformThreadHandle::Handle thread_handle =
        PlatformThread::CurrentHandle().platform_handle();
    internal::AssertMemoryPriority(thread_handle, memory_priority_);
  }

  LONG memory_priority_;
};
}  // namespace

}  // namespace base
