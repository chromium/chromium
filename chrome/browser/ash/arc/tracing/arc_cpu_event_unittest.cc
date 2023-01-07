// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_cpu_event.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcCpuEventTest = testing::Test;
using Type = ArcCpuEvent::Type;

namespace {

bool CheckTrasition(Type from_type,
                    uint32_t from_tid,
                    Type to_type,
                    uint32_t to_tid) {
  CpuEvents cpu_events;
  const bool added =
      AddCpuEvent(&cpu_events, 1000 /* timestamp */, from_type, from_tid);
  DCHECK(added);
  DCHECK(cpu_events.size() == 1);
  if (!AddCpuEvent(&cpu_events, 1001 /* timestamp */, to_type, to_tid)) {
    DCHECK(cpu_events.size() == 1);
    return false;
  }
  DCHECK(cpu_events.size() == 2);
  return true;
}

}  // namespace

TEST_F(ArcCpuEventTest, Generic) {
  constexpr int idle_tid = 0;
  constexpr int real_tid = 1;

  AllCpuEvents all_cpu_events;
  EXPECT_TRUE(AddAllCpuEvent(&all_cpu_events, 3 /* cpu_id */,
                             1000 /* timestamp */, Type::kIdleIn, idle_tid));
  ASSERT_EQ(4U, all_cpu_events.size());
  EXPECT_EQ(0U, all_cpu_events[0].size());
  EXPECT_EQ(0U, all_cpu_events[1].size());
  EXPECT_EQ(0U, all_cpu_events[2].size());
  EXPECT_EQ(1U, all_cpu_events[3].size());

  // Timestamp is before but this is separate CPU events band.
  EXPECT_TRUE(AddAllCpuEvent(&all_cpu_events, 0 /* cpu_id */,
                             999 /* timestamp */, Type::kIdleIn, idle_tid));
  ASSERT_EQ(4U, all_cpu_events.size());
  EXPECT_EQ(1U, all_cpu_events[0].size());
  EXPECT_EQ(0U, all_cpu_events[1].size());
  EXPECT_EQ(0U, all_cpu_events[2].size());
  EXPECT_EQ(1U, all_cpu_events[3].size());

  // Broken timestamp.
  EXPECT_FALSE(AddAllCpuEvent(&all_cpu_events, 0 /* cpu_id */,
                              998 /* timestamp */, Type::kIdleOut, idle_tid));

  // Validate transitions.
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleIn, idle_tid, Type::kIdleIn, idle_tid));
  EXPECT_TRUE(
      CheckTrasition(Type::kIdleIn, idle_tid, Type::kIdleOut, idle_tid));
  EXPECT_TRUE(CheckTrasition(Type::kIdleIn, idle_tid, Type::kWakeUp, real_tid));
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleIn, idle_tid, Type::kActive, real_tid));

  EXPECT_TRUE(
      CheckTrasition(Type::kIdleOut, idle_tid, Type::kIdleIn, idle_tid));
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleOut, idle_tid, Type::kIdleOut, idle_tid));
  EXPECT_TRUE(
      CheckTrasition(Type::kIdleOut, idle_tid, Type::kWakeUp, real_tid));
  EXPECT_TRUE(
      CheckTrasition(Type::kIdleOut, idle_tid, Type::kActive, real_tid));

  EXPECT_TRUE(CheckTrasition(Type::kWakeUp, real_tid, Type::kIdleIn, idle_tid));
  EXPECT_TRUE(
      CheckTrasition(Type::kWakeUp, real_tid, Type::kIdleOut, idle_tid));
  EXPECT_TRUE(CheckTrasition(Type::kWakeUp, real_tid, Type::kWakeUp, real_tid));
  EXPECT_TRUE(CheckTrasition(Type::kWakeUp, real_tid, Type::kActive, real_tid));

  EXPECT_TRUE(CheckTrasition(Type::kActive, real_tid, Type::kIdleIn, idle_tid));
  EXPECT_FALSE(
      CheckTrasition(Type::kActive, real_tid, Type::kIdleOut, idle_tid));
  EXPECT_TRUE(CheckTrasition(Type::kActive, real_tid, Type::kWakeUp, real_tid));
  EXPECT_TRUE(CheckTrasition(Type::kActive, real_tid, Type::kActive, real_tid));

  // tid is not idle for idle events.
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleIn, idle_tid, Type::kIdleOut, real_tid));
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleOut, idle_tid, Type::kIdleIn, real_tid));
  // tid is idle for wake-up event.
  EXPECT_FALSE(
      CheckTrasition(Type::kIdleIn, idle_tid, Type::kWakeUp, idle_tid));
}

}  // namespace arc
