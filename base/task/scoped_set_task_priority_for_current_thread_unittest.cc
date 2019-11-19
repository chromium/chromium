// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/scoped_set_task_priority_for_current_thread.h"

#include "base/task/task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

TEST(ScopedSetTaskPriorityForCurrentThreadTest,
     ScopedSetTaskPriorityForCurrentThread) {
  EXPECT_EQ(TaskPriority::USER_BLOCKING, GetTaskPriorityForCurrentThread());
  {
    ScopedSetTaskPriorityForCurrentThread
        scoped_set_task_priority_for_current_thread(TaskPriority::BEST_EFFORT);
    EXPECT_EQ(TaskPriority::BEST_EFFORT, GetTaskPriorityForCurrentThread());
  }
  EXPECT_EQ(TaskPriority::USER_BLOCKING, GetTaskPriorityForCurrentThread());
}

}  // namespace internal
}  // namespace base
