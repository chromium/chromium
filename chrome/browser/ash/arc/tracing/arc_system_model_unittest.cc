// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_system_model.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcSystemModelTest = testing::Test;

TEST_F(ArcSystemModelTest, TrimByTimestampCPU) {
  ArcSystemModel model;

  constexpr uint64_t trim_timestamp = 25;
  constexpr int idle_tid = 0;
  constexpr int non_idle_tid = 100;

  // Second event is before |trim_timestamp|. First event should be discard,
  // second should be preserved but with clamped timestamp to |trim_timestamp|.
  AddAllCpuEvent(&model.all_cpu_events(), 0 /* cpu_id */, 10 /* timestamp */,
                 ArcCpuEvent::Type::kIdleIn, idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 0 /* cpu_id */, 20 /* timestamp */,
                 ArcCpuEvent::Type::kWakeUp, non_idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 0 /* cpu_id */, 30 /* timestamp */,
                 ArcCpuEvent::Type::kIdleOut, idle_tid);

  // All events are after |trim_timestamp|. Everything should be preserved.
  AddAllCpuEvent(&model.all_cpu_events(), 1 /* cpu_id */, 32 /* timestamp */,
                 ArcCpuEvent::Type::kIdleIn, idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 1 /* cpu_id */, 42 /* timestamp */,
                 ArcCpuEvent::Type::kWakeUp, non_idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 1 /* cpu_id */, 52 /* timestamp */,
                 ArcCpuEvent::Type::kIdleOut, idle_tid);

  // Second event is exactly |on trim_timestamp|. First even should be
  // discarded.
  AddAllCpuEvent(&model.all_cpu_events(), 2 /* cpu_id */, 15 /* timestamp */,
                 ArcCpuEvent::Type::kIdleIn, idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 2 /* cpu_id */, 25 /* timestamp */,
                 ArcCpuEvent::Type::kWakeUp, non_idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 2 /* cpu_id */, 35 /* timestamp */,
                 ArcCpuEvent::Type::kIdleOut, idle_tid);

  // All events are before |trim_timestamp|. Last one should be preserved but
  // with clamped timestamp to |trim_timestamp|.
  AddAllCpuEvent(&model.all_cpu_events(), 3 /* cpu_id */, 10 /* timestamp */,
                 ArcCpuEvent::Type::kIdleIn, idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 3 /* cpu_id */, 15 /* timestamp */,
                 ArcCpuEvent::Type::kWakeUp, non_idle_tid);
  AddAllCpuEvent(&model.all_cpu_events(), 3 /* cpu_id */, 20 /* timestamp */,
                 ArcCpuEvent::Type::kIdleOut, idle_tid);

  model.Trim(trim_timestamp);

  ASSERT_EQ(4U, model.all_cpu_events().size());
  const CpuEvents& cpu_events_0 = model.all_cpu_events()[0 /* cpu_id */];
  const CpuEvents& cpu_events_1 = model.all_cpu_events()[1 /* cpu_id */];
  const CpuEvents& cpu_events_2 = model.all_cpu_events()[2 /* cpu_id */];
  const CpuEvents& cpu_events_3 = model.all_cpu_events()[3 /* cpu_id */];

  ASSERT_EQ(2U, cpu_events_0.size());
  EXPECT_EQ(ArcCpuEvent::Type::kWakeUp, cpu_events_0[0].type);
  EXPECT_EQ(trim_timestamp, cpu_events_0[0].timestamp);

  ASSERT_EQ(3U, cpu_events_1.size());
  EXPECT_EQ(ArcCpuEvent::Type::kIdleIn, cpu_events_1[0].type);
  EXPECT_EQ(32u, cpu_events_1[0].timestamp);

  ASSERT_EQ(2U, cpu_events_2.size());
  EXPECT_EQ(ArcCpuEvent::Type::kWakeUp, cpu_events_2[0].type);
  EXPECT_EQ(trim_timestamp, cpu_events_2[0].timestamp);

  ASSERT_EQ(1U, cpu_events_3.size());
  EXPECT_EQ(ArcCpuEvent::Type::kIdleOut, cpu_events_3[0].type);
  EXPECT_EQ(trim_timestamp, cpu_events_3[0].timestamp);
}

TEST_F(ArcSystemModelTest, TrimByTimestampMemory) {
  ArcSystemModel model;

  constexpr uint64_t trim_timestamp = 25;

  // First of ArcValueEvent::Type::kMemTotal should be clamped to
  // |trim_timestamp|.
  model.memory_events().emplace_back(
      0 /* timestamp */, ArcValueEvent::Type::kMemTotal, 100 /* value */);
  model.memory_events().emplace_back(
      0 /* timestamp */, ArcValueEvent::Type::kMemUsed, 20 /* value */);
  model.memory_events().emplace_back(
      0 /* timestamp */, ArcValueEvent::Type::kSwapRead, 0 /* value */);

  model.memory_events().emplace_back(
      10 /* timestamp */, ArcValueEvent::Type::kSwapRead, 1 /* value */);

  model.memory_events().emplace_back(
      20 /* timestamp */, ArcValueEvent::Type::kSwapRead, 2 /* value */);

  // Second of |ArcValueEvent::Type::kMemUsed| is exactly on |trim_timestamp|.
  // First |ArcValueEvent::Type::kMemUsed| should be discarded.
  model.memory_events().emplace_back(
      25 /* timestamp */, ArcValueEvent::Type::kMemUsed, 30 /* value */);

  // We have 3 |ArcValueEvent::Type::kSwapRead| before. Two first should be
  // discarded and third one should be clamped to |trim_timestamp|.
  model.memory_events().emplace_back(
      30 /* timestamp */, ArcValueEvent::Type::kSwapRead, 3 /* value */);

  model.memory_events().emplace_back(
      40 /* timestamp */, ArcValueEvent::Type::kSwapRead, 4 /* value */);

  model.memory_events().emplace_back(
      50 /* timestamp */, ArcValueEvent::Type::kMemTotal, 100 /* value */);
  model.memory_events().emplace_back(
      50 /* timestamp */, ArcValueEvent::Type::kMemUsed, 40 /* value */);
  model.memory_events().emplace_back(
      50 /* timestamp */, ArcValueEvent::Type::kSwapRead, 5 /* value */);

  EXPECT_EQ(11u, model.memory_events().size());

  model.Trim(trim_timestamp);

  ASSERT_EQ(8u, model.memory_events().size());
  EXPECT_EQ(ArcValueEvent(trim_timestamp, ArcValueEvent::Type::kMemTotal, 100),
            model.memory_events()[0]);
  EXPECT_EQ(ArcValueEvent(trim_timestamp, ArcValueEvent::Type::kSwapRead, 2),
            model.memory_events()[1]);
  EXPECT_EQ(ArcValueEvent(trim_timestamp, ArcValueEvent::Type::kMemUsed, 30),
            model.memory_events()[2]);

  EXPECT_EQ(ArcValueEvent(30, ArcValueEvent::Type::kSwapRead, 3),
            model.memory_events()[3]);
  EXPECT_EQ(ArcValueEvent(40, ArcValueEvent::Type::kSwapRead, 4),
            model.memory_events()[4]);

  EXPECT_EQ(ArcValueEvent(50, ArcValueEvent::Type::kMemTotal, 100),
            model.memory_events()[5]);
  EXPECT_EQ(ArcValueEvent(50, ArcValueEvent::Type::kMemUsed, 40),
            model.memory_events()[6]);
  EXPECT_EQ(ArcValueEvent(50, ArcValueEvent::Type::kSwapRead, 5),
            model.memory_events()[7]);
}

TEST_F(ArcSystemModelTest, CloseRange) {
  ArcSystemModel model;

  model.memory_events().emplace_back(
      0 /* timestamp */, ArcValueEvent::Type::kGpuFrequency, 100 /* value */);
  model.memory_events().emplace_back(
      0 /* timestamp */, ArcValueEvent::Type::kCpuTemperature, 20 /* value */);
  model.memory_events().emplace_back(
      100 /* timestamp */, ArcValueEvent::Type::kGpuFrequency, 150 /* value */);
  model.memory_events().emplace_back(200 /* timestamp */,
                                     ArcValueEvent::Type::kCpuTemperature,
                                     50 /* value */);

  EXPECT_EQ(4u, model.memory_events().size());

  model.CloseRangeForValueEvents(200 /* timestamp */);

  // kGpuFrequency is extended.
  ASSERT_EQ(5u, model.memory_events().size());
  EXPECT_EQ(ArcValueEvent(200 /* timestamp */,
                          ArcValueEvent::Type::kGpuFrequency, 150 /* value */),
            model.memory_events().back());
}

}  // namespace arc
