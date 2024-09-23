// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/task/thread_pool/task_source_sort_key.h"

#include <iterator>

#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

// Keys are manually ordered from the least important to the most important.
const TaskSourceSortKey kTestKeys[] = {
    {TaskPriority::BEST_EFFORT, TimeTicks() + Seconds(2000)},
    {TaskPriority::BEST_EFFORT, TimeTicks() + Seconds(1000)},
    {TaskPriority::USER_VISIBLE, TimeTicks() + Seconds(2000), 1},
    {TaskPriority::USER_VISIBLE, TimeTicks() + Seconds(1000), 1},
    {TaskPriority::USER_VISIBLE, TimeTicks() + Seconds(2000)},
    {TaskPriority::USER_VISIBLE, TimeTicks() + Seconds(1000)},
    {TaskPriority::USER_BLOCKING, TimeTicks() + Seconds(2000)},
    {TaskPriority::USER_BLOCKING, TimeTicks() + Seconds(1000)},
};

}  // namespace

TEST(TaskSourceSortKeyTest, OperatorLessThan) {
  for (size_t i = 0; i < std::size(kTestKeys); i++) {
    // All the entries before the index of the current key are smaller.
    for (size_t j = 0; j < i; j++)
      EXPECT_LT(kTestKeys[j], kTestKeys[i]);

    // All the other entries (including itself) are not smaller than the current
    // key.
    for (size_t j = i; j < std::size(kTestKeys); j++)
      EXPECT_FALSE(kTestKeys[j] < kTestKeys[i]);
  }
}

TEST(TaskSourceSortKeyTest, OperatorEqual) {
  // Compare each test key to every other key. They will be equal only when
  // their index is the same.
  for (size_t i = 0; i < std::size(kTestKeys); i++) {
    for (size_t j = 0; j < std::size(kTestKeys); j++) {
      if (i == j)
        EXPECT_EQ(kTestKeys[i], kTestKeys[j]);
      else
        EXPECT_FALSE(kTestKeys[i] == kTestKeys[j]);
    }
  }
}

TEST(TaskSourceSortKeyTest, OperatorNotEqual) {
  // Compare each test key to every other key. They will not be equal only when
  // their index is different.
  for (size_t i = 0; i < std::size(kTestKeys); i++) {
    for (size_t j = 0; j < std::size(kTestKeys); j++) {
      if (i != j)
        EXPECT_NE(kTestKeys[i], kTestKeys[j]);
      else
        EXPECT_FALSE(kTestKeys[i] != kTestKeys[j]);
    }
  }
}

}  // namespace internal
}  // namespace base
