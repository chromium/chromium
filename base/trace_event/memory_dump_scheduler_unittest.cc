// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_scheduler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::_;

namespace base {
namespace trace_event {

namespace {

// Wrapper to use gmock on a callback.
struct CallbackWrapper {
  MOCK_METHOD1(OnTick, void(MemoryDumpLevelOfDetail));
};

}  // namespace

class MemoryDumpSchedulerTest : public testing::Test {
 public:
  MemoryDumpSchedulerTest()
      : testing::Test(),
        evt_(WaitableEvent::ResetPolicy::MANUAL,
             WaitableEvent::InitialState::NOT_SIGNALED),
        bg_thread_("MemoryDumpSchedulerTest Thread") {
    bg_thread_.Start();
  }

 protected:
  MemoryDumpScheduler scheduler_;
  WaitableEvent evt_;
  CallbackWrapper on_tick_;
  Thread bg_thread_;
};

TEST_F(MemoryDumpSchedulerTest, SingleTrigger) {
  const uint32_t kPeriodMs = 1;
  const auto kLevelOfDetail = MemoryDumpLevelOfDetail::kDetailed;
  const uint32_t kTicks = 5;
  MemoryDumpScheduler::Config config;
  config.triggers.push_back({kLevelOfDetail, kPeriodMs});
  config.callback =
      BindRepeating(&CallbackWrapper::OnTick, Unretained(&on_tick_));

  testing::InSequence sequence;
  EXPECT_CALL(on_tick_, OnTick(_)).Times(kTicks - 1);
  EXPECT_CALL(on_tick_, OnTick(_))
      .WillRepeatedly(Invoke(
          [this, kLevelOfDetail](MemoryDumpLevelOfDetail level_of_detail) {
            EXPECT_EQ(kLevelOfDetail, level_of_detail);
            this->evt_.Signal();
          }));

  // Check that Stop() before Start() doesn't cause any error.
  scheduler_.Stop();

  const TimeTicks tstart = TimeTicks::Now();
  scheduler_.Start(config, bg_thread_.task_runner());
  evt_.Wait();
  const double time_ms = (TimeTicks::Now() - tstart).InMillisecondsF();

  // It takes N-1 ms to perform N ticks of 1ms each.
  EXPECT_GE(time_ms, kPeriodMs * (kTicks - 1));

  // Check that stopping twice doesn't cause any problems.
  scheduler_.Stop();
  scheduler_.Stop();
}

TEST_F(MemoryDumpSchedulerTest, MultipleTriggers) {
  const uint32_t kPeriodLightMs = 3;
  const uint32_t kPeriodDetailedMs = 9;
  MemoryDumpScheduler::Config config;
  const MemoryDumpLevelOfDetail kLight = MemoryDumpLevelOfDetail::kLight;
  const MemoryDumpLevelOfDetail kDetailed = MemoryDumpLevelOfDetail::kDetailed;
  config.triggers.push_back({kLight, kPeriodLightMs});
  config.triggers.push_back({kDetailed, kPeriodDetailedMs});
  config.callback =
      BindRepeating(&CallbackWrapper::OnTick, Unretained(&on_tick_));

  TimeTicks t1, t2, t3;

  testing::InSequence sequence;
  EXPECT_CALL(on_tick_, OnTick(kDetailed))
      .WillOnce(
          Invoke([&t1](MemoryDumpLevelOfDetail) { t1 = TimeTicks::Now(); }));
  EXPECT_CALL(on_tick_, OnTick(kLight)).Times(1);
  EXPECT_CALL(on_tick_, OnTick(kLight)).Times(1);
  EXPECT_CALL(on_tick_, OnTick(kDetailed))
      .WillOnce(
          Invoke([&t2](MemoryDumpLevelOfDetail) { t2 = TimeTicks::Now(); }));
  EXPECT_CALL(on_tick_, OnTick(kLight))
      .WillOnce(
          Invoke([&t3](MemoryDumpLevelOfDetail) { t3 = TimeTicks::Now(); }));

  // Rationale for WillRepeatedly and not just WillOnce: Extra ticks might
  // happen if the Stop() takes time. Not an interesting case, but we need to
  // avoid gmock to shout in that case.
  EXPECT_CALL(on_tick_, OnTick(_))
      .WillRepeatedly(
          Invoke([this](MemoryDumpLevelOfDetail) { this->evt_.Signal(); }));

  scheduler_.Start(config, bg_thread_.task_runner());
  evt_.Wait();
  scheduler_.Stop();
  EXPECT_GE((t2 - t1).InMillisecondsF(), kPeriodDetailedMs);
  EXPECT_GE((t3 - t2).InMillisecondsF(), kPeriodLightMs);
}

TEST_F(MemoryDumpSchedulerTest, StartStopQuickly) {
  const uint32_t kPeriodMs = 3;
  const uint32_t kQuickIterations = 5;
  const uint32_t kDetailedTicks = 10;

  MemoryDumpScheduler::Config light_config;
  light_config.triggers.push_back({MemoryDumpLevelOfDetail::kLight, kPeriodMs});
  light_config.callback =
      BindRepeating(&CallbackWrapper::OnTick, Unretained(&on_tick_));

  MemoryDumpScheduler::Config detailed_config;
  detailed_config.triggers.push_back(
      {MemoryDumpLevelOfDetail::kDetailed, kPeriodMs});
  detailed_config.callback =
      BindRepeating(&CallbackWrapper::OnTick, Unretained(&on_tick_));

  testing::InSequence sequence;
  EXPECT_CALL(on_tick_, OnTick(MemoryDumpLevelOfDetail::kLight))
      .Times(AtMost(kQuickIterations));
  EXPECT_CALL(on_tick_, OnTick(MemoryDumpLevelOfDetail::kDetailed))
      .Times(kDetailedTicks - 1);
  EXPECT_CALL(on_tick_, OnTick(MemoryDumpLevelOfDetail::kDetailed))
      .WillRepeatedly(
          Invoke([this](MemoryDumpLevelOfDetail) { this->evt_.Signal(); }));

  const TimeTicks tstart = TimeTicks::Now();
  for (unsigned int i = 0; i < kQuickIterations; i++) {
    scheduler_.Start(light_config, bg_thread_.task_runner());
    scheduler_.Stop();
  }

  scheduler_.Start(detailed_config, bg_thread_.task_runner());

  evt_.Wait();
  const double time_ms = (TimeTicks::Now() - tstart).InMillisecondsF();
  scheduler_.Stop();

  // It takes N-1 ms to perform N ticks of 1ms each.
  EXPECT_GE(time_ms, kPeriodMs * (kDetailedTicks - 1));
}

TEST_F(MemoryDumpSchedulerTest, StopAndStartOnAnotherThread) {
  const uint32_t kPeriodMs = 1;
  const uint32_t kTicks = 3;
  MemoryDumpScheduler::Config config;
  config.triggers.push_back({MemoryDumpLevelOfDetail::kDetailed, kPeriodMs});
  config.callback =
      BindRepeating(&CallbackWrapper::OnTick, Unretained(&on_tick_));

  auto expected_task_runner = bg_thread_.task_runner();
  testing::InSequence sequence;
  EXPECT_CALL(on_tick_, OnTick(_)).Times(kTicks - 1);
  EXPECT_CALL(on_tick_, OnTick(_))
      .WillRepeatedly(
          Invoke([this, expected_task_runner](MemoryDumpLevelOfDetail) {
            EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
            this->evt_.Signal();
          }));

  scheduler_.Start(config, bg_thread_.task_runner());
  evt_.Wait();
  scheduler_.Stop();
  bg_thread_.Stop();

  Thread bg_thread_2("MemoryDumpSchedulerTest Thread 2");
  bg_thread_2.Start();
  evt_.Reset();
  expected_task_runner = bg_thread_2.task_runner();
  EXPECT_CALL(on_tick_, OnTick(_)).Times(kTicks - 1);
  EXPECT_CALL(on_tick_, OnTick(_))
      .WillRepeatedly(
          Invoke([this, expected_task_runner](MemoryDumpLevelOfDetail) {
            EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
            this->evt_.Signal();
          }));
  scheduler_.Start(config, bg_thread_2.task_runner());
  evt_.Wait();
  scheduler_.Stop();
}

}  // namespace trace_event
}  // namespace base
