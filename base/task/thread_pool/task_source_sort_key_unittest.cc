// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_source_sort_key.h"

#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

TEST(TaskSourceSortKeyTest, OperatorLessThanOrEqual) {
  TaskSourceSortKey key_a(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_b(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_c(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_d(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_e(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000), 1);
  TaskSourceSortKey key_f(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000), 1);
  TaskSourceSortKey key_g(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_h(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(2000));

  EXPECT_LE(key_a, key_a);
  EXPECT_FALSE(key_b <= key_a);
  EXPECT_FALSE(key_c <= key_a);
  EXPECT_FALSE(key_d <= key_a);
  EXPECT_FALSE(key_e <= key_a);
  EXPECT_FALSE(key_f <= key_a);
  EXPECT_FALSE(key_g <= key_a);
  EXPECT_FALSE(key_h <= key_a);

  EXPECT_LE(key_a, key_b);
  EXPECT_LE(key_b, key_b);
  EXPECT_FALSE(key_c <= key_b);
  EXPECT_FALSE(key_d <= key_b);
  EXPECT_FALSE(key_e <= key_b);
  EXPECT_FALSE(key_f <= key_b);
  EXPECT_FALSE(key_g <= key_b);
  EXPECT_FALSE(key_h <= key_b);

  EXPECT_LE(key_a, key_c);
  EXPECT_LE(key_b, key_c);
  EXPECT_LE(key_c, key_c);
  EXPECT_FALSE(key_d <= key_c);
  EXPECT_FALSE(key_e <= key_c);
  EXPECT_FALSE(key_f <= key_c);
  EXPECT_FALSE(key_g <= key_c);
  EXPECT_FALSE(key_h <= key_c);

  EXPECT_LE(key_a, key_d);
  EXPECT_LE(key_b, key_d);
  EXPECT_LE(key_c, key_d);
  EXPECT_LE(key_d, key_d);
  EXPECT_FALSE(key_e <= key_d);
  EXPECT_FALSE(key_f <= key_d);
  EXPECT_FALSE(key_g <= key_d);
  EXPECT_FALSE(key_h <= key_d);

  EXPECT_LE(key_a, key_e);
  EXPECT_LE(key_b, key_e);
  EXPECT_LE(key_c, key_e);
  EXPECT_LE(key_d, key_e);
  EXPECT_LE(key_e, key_e);
  EXPECT_FALSE(key_f <= key_e);
  EXPECT_FALSE(key_g <= key_e);
  EXPECT_FALSE(key_h <= key_e);

  EXPECT_LE(key_a, key_f);
  EXPECT_LE(key_b, key_f);
  EXPECT_LE(key_c, key_f);
  EXPECT_LE(key_d, key_f);
  EXPECT_LE(key_e, key_f);
  EXPECT_LE(key_f, key_f);
  EXPECT_FALSE(key_g <= key_f);
  EXPECT_FALSE(key_h <= key_f);

  EXPECT_LE(key_a, key_g);
  EXPECT_LE(key_b, key_g);
  EXPECT_LE(key_c, key_g);
  EXPECT_LE(key_d, key_g);
  EXPECT_LE(key_e, key_g);
  EXPECT_LE(key_f, key_g);
  EXPECT_LE(key_g, key_g);
  EXPECT_FALSE(key_h <= key_g);

  EXPECT_LE(key_a, key_h);
  EXPECT_LE(key_b, key_h);
  EXPECT_LE(key_c, key_h);
  EXPECT_LE(key_d, key_h);
  EXPECT_LE(key_e, key_h);
  EXPECT_LE(key_f, key_h);
  EXPECT_LE(key_g, key_h);
  EXPECT_LE(key_h, key_h);
}

TEST(TaskSourceSortKeyTest, OperatorEqual) {
  TaskSourceSortKey key_a(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_b(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_c(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_d(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_e(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000), 1);
  TaskSourceSortKey key_f(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000), 1);
  TaskSourceSortKey key_g(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_h(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(2000));

  EXPECT_EQ(key_a, key_a);
  EXPECT_FALSE(key_b == key_a);
  EXPECT_FALSE(key_c == key_a);
  EXPECT_FALSE(key_d == key_a);
  EXPECT_FALSE(key_e == key_a);
  EXPECT_FALSE(key_f == key_a);
  EXPECT_FALSE(key_g == key_a);
  EXPECT_FALSE(key_h == key_a);

  EXPECT_FALSE(key_a == key_b);
  EXPECT_EQ(key_b, key_b);
  EXPECT_FALSE(key_c == key_b);
  EXPECT_FALSE(key_d == key_b);
  EXPECT_FALSE(key_e == key_b);
  EXPECT_FALSE(key_f == key_b);
  EXPECT_FALSE(key_g == key_b);
  EXPECT_FALSE(key_h == key_b);

  EXPECT_FALSE(key_a == key_c);
  EXPECT_FALSE(key_b == key_c);
  EXPECT_EQ(key_c, key_c);
  EXPECT_FALSE(key_d == key_c);
  EXPECT_FALSE(key_e == key_c);
  EXPECT_FALSE(key_f == key_c);
  EXPECT_FALSE(key_g == key_c);
  EXPECT_FALSE(key_h == key_c);

  EXPECT_FALSE(key_a == key_d);
  EXPECT_FALSE(key_b == key_d);
  EXPECT_FALSE(key_c == key_d);
  EXPECT_EQ(key_d, key_d);
  EXPECT_FALSE(key_e == key_d);
  EXPECT_FALSE(key_f == key_d);
  EXPECT_FALSE(key_g == key_d);
  EXPECT_FALSE(key_h == key_d);

  EXPECT_FALSE(key_a == key_e);
  EXPECT_FALSE(key_b == key_e);
  EXPECT_FALSE(key_c == key_e);
  EXPECT_FALSE(key_d == key_e);
  EXPECT_EQ(key_e, key_e);
  EXPECT_FALSE(key_f == key_e);
  EXPECT_FALSE(key_g == key_e);
  EXPECT_FALSE(key_h == key_e);

  EXPECT_FALSE(key_a == key_f);
  EXPECT_FALSE(key_b == key_f);
  EXPECT_FALSE(key_c == key_f);
  EXPECT_FALSE(key_d == key_f);
  EXPECT_FALSE(key_e == key_f);
  EXPECT_EQ(key_f, key_f);
  EXPECT_FALSE(key_g == key_f);
  EXPECT_FALSE(key_h == key_f);

  EXPECT_FALSE(key_a == key_g);
  EXPECT_FALSE(key_b == key_g);
  EXPECT_FALSE(key_c == key_g);
  EXPECT_FALSE(key_d == key_g);
  EXPECT_FALSE(key_e == key_g);
  EXPECT_FALSE(key_f == key_g);
  EXPECT_EQ(key_g, key_g);
  EXPECT_FALSE(key_h == key_g);

  EXPECT_FALSE(key_a == key_h);
  EXPECT_FALSE(key_b == key_h);
  EXPECT_FALSE(key_c == key_h);
  EXPECT_FALSE(key_d == key_h);
  EXPECT_FALSE(key_e == key_h);
  EXPECT_FALSE(key_f == key_h);
  EXPECT_FALSE(key_g == key_h);
  EXPECT_EQ(key_h, key_h);
}

TEST(TaskSourceSortKeyTest, OperatorNotEqual) {
  TaskSourceSortKey key_a(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_b(TaskPriority::USER_BLOCKING,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_c(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_d(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000));
  TaskSourceSortKey key_e(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(1000), 1);
  TaskSourceSortKey key_f(TaskPriority::USER_VISIBLE,
                          TimeTicks::FromInternalValue(2000), 1);
  TaskSourceSortKey key_g(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(1000));
  TaskSourceSortKey key_h(TaskPriority::BEST_EFFORT,
                          TimeTicks::FromInternalValue(2000));

  EXPECT_FALSE(key_a != key_a);
  EXPECT_NE(key_b, key_a);
  EXPECT_NE(key_c, key_a);
  EXPECT_NE(key_d, key_a);
  EXPECT_NE(key_e, key_a);
  EXPECT_NE(key_f, key_a);
  EXPECT_NE(key_g, key_a);
  EXPECT_NE(key_h, key_a);

  EXPECT_NE(key_a, key_b);
  EXPECT_FALSE(key_b != key_b);
  EXPECT_NE(key_c, key_b);
  EXPECT_NE(key_d, key_b);
  EXPECT_NE(key_e, key_b);
  EXPECT_NE(key_f, key_b);
  EXPECT_NE(key_g, key_b);
  EXPECT_NE(key_h, key_b);

  EXPECT_NE(key_a, key_c);
  EXPECT_NE(key_b, key_c);
  EXPECT_FALSE(key_c != key_c);
  EXPECT_NE(key_d, key_c);
  EXPECT_NE(key_e, key_c);
  EXPECT_NE(key_f, key_c);
  EXPECT_NE(key_g, key_c);
  EXPECT_NE(key_h, key_c);

  EXPECT_NE(key_a, key_d);
  EXPECT_NE(key_b, key_d);
  EXPECT_NE(key_c, key_d);
  EXPECT_FALSE(key_d != key_d);
  EXPECT_NE(key_e, key_d);
  EXPECT_NE(key_f, key_d);
  EXPECT_NE(key_g, key_d);
  EXPECT_NE(key_h, key_d);

  EXPECT_NE(key_a, key_e);
  EXPECT_NE(key_b, key_e);
  EXPECT_NE(key_c, key_e);
  EXPECT_NE(key_d, key_e);
  EXPECT_FALSE(key_e != key_e);
  EXPECT_NE(key_f, key_e);
  EXPECT_NE(key_g, key_e);
  EXPECT_NE(key_h, key_e);

  EXPECT_NE(key_a, key_f);
  EXPECT_NE(key_b, key_f);
  EXPECT_NE(key_c, key_f);
  EXPECT_NE(key_d, key_f);
  EXPECT_NE(key_e, key_f);
  EXPECT_FALSE(key_f != key_f);
  EXPECT_NE(key_g, key_f);
  EXPECT_NE(key_h, key_f);

  EXPECT_NE(key_a, key_g);
  EXPECT_NE(key_b, key_g);
  EXPECT_NE(key_c, key_g);
  EXPECT_NE(key_d, key_g);
  EXPECT_NE(key_e, key_g);
  EXPECT_NE(key_f, key_g);
  EXPECT_FALSE(key_g != key_g);
  EXPECT_NE(key_h, key_g);

  EXPECT_NE(key_a, key_h);
  EXPECT_NE(key_b, key_h);
  EXPECT_NE(key_c, key_h);
  EXPECT_NE(key_d, key_h);
  EXPECT_NE(key_e, key_h);
  EXPECT_NE(key_f, key_h);
  EXPECT_NE(key_g, key_h);
  EXPECT_FALSE(key_h != key_h);
}

}  // namespace internal
}  // namespace base
