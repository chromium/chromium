// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedThreadPriorityTest : public testing::Test {
 protected:
  void SetUp() override {
#if defined(OS_WIN)
    scoped_features_.InitWithFeatures({kBoostThreadPriorityOnLibraryLoading},
                                      {});
#endif  // OS_WIN

    // Ensures the default thread priority is set.
    ASSERT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }

 private:
  test::ScopedFeatureList scoped_features_;
};

TEST_F(ScopedThreadPriorityTest, WithoutPriorityBoost) {
  // Validates that a thread at normal priority keep the same priority.
  {
    ScopedThreadMayLoadLibraryOnBackgroundThread priority_boost(FROM_HERE);
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
  EXPECT_EQ(ThreadPriority::NORMAL, PlatformThread::GetCurrentThreadPriority());
}

#if defined(OS_WIN)
TEST_F(ScopedThreadPriorityTest, WithPriorityBoost) {
  // Validates that a thread at background priority is boosted to normal
  // priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);
  {
    ScopedThreadMayLoadLibraryOnBackgroundThread priority_boost(FROM_HERE);
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
  EXPECT_EQ(ThreadPriority::BACKGROUND,
            PlatformThread::GetCurrentThreadPriority());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}
#endif  // OS_WIN

#if defined(OS_WIN)
TEST_F(ScopedThreadPriorityTest, NestedScope) {
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);

  {
    ScopedThreadMayLoadLibraryOnBackgroundThread priority_boost(FROM_HERE);
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
    {
      ScopedThreadMayLoadLibraryOnBackgroundThread nested_priority_boost(
          FROM_HERE);
      EXPECT_EQ(ThreadPriority::NORMAL,
                PlatformThread::GetCurrentThreadPriority());
    }
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }

  EXPECT_EQ(ThreadPriority::BACKGROUND,
            PlatformThread::GetCurrentThreadPriority());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}
#endif  // OS_WIN

}  // namespace base
