// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/mock_time_message_pump.h"

#include "base/message_loop/message_pump.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace sequence_manager {
namespace {

using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

class MockMessagePumpDelegate : public MessagePump::Delegate {
 public:
  MOCK_METHOD0(OnBeginWorkItem, void());
  MOCK_METHOD1(OnEndWorkItem, void(int));
  MOCK_METHOD0(BeforeWait, void());
  MOCK_METHOD0(BeginNativeWorkBeforeDoWork, void());
  MOCK_METHOD0(DoWork, NextWorkInfo());
  MOCK_METHOD0(DoIdleWork, void());
  MOCK_METHOD0(RunDepth, int());
};

MessagePump::Delegate::NextWorkInfo NextWorkInfo(TimeTicks delayed_run_time) {
  MessagePump::Delegate::NextWorkInfo info;
  info.delayed_run_time = delayed_run_time;
  return info;
}

TEST(MockMessagePumpTest, KeepsRunningIfNotAllowedToAdvanceTime) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kStartTime = mock_clock.NowTicks();
  const auto kFutureTime = kStartTime + Seconds(42);

  EXPECT_CALL(delegate, DoWork)
      .WillOnce(Return(NextWorkInfo(TimeTicks())))
      .WillOnce(Return(NextWorkInfo(TimeTicks())))
      .WillOnce(Return(NextWorkInfo(kFutureTime)));
  EXPECT_CALL(delegate, DoIdleWork).WillOnce(Invoke([&] {
    pump.Quit();
  }));

  pump.Run(&delegate);

  EXPECT_THAT(mock_clock.NowTicks(), Eq(kStartTime));
}

TEST(MockMessagePumpTest, AdvancesTimeAsAllowed) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kStartTime = mock_clock.NowTicks();
  const auto kEndTime = kStartTime + Seconds(2);

  pump.SetAllowTimeToAutoAdvanceUntil(kEndTime);
  pump.SetStopWhenMessagePumpIsIdle(true);
  EXPECT_CALL(delegate, DoWork).Times(3).WillRepeatedly(Invoke([&] {
    return NextWorkInfo(mock_clock.NowTicks() + Seconds(1));
  }));
  EXPECT_CALL(delegate, DoIdleWork).Times(3);

  pump.Run(&delegate);

  EXPECT_THAT(mock_clock.NowTicks(), Eq(kEndTime));
}

TEST(MockMessagePumpTest, CanQuitAfterMaybeDoWork) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);

  pump.SetQuitAfterDoWork(true);
  EXPECT_CALL(delegate, DoWork).WillOnce(Return(NextWorkInfo(TimeTicks())));

  pump.Run(&delegate);
}

TEST(MockMessagePumpTest, AdvancesUntilAllowedTime) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kStartTime = mock_clock.NowTicks();
  const auto kEndTime = kStartTime + Seconds(2);
  const auto kNextDelayedWorkTime = kEndTime + Seconds(2);

  pump.SetAllowTimeToAutoAdvanceUntil(kEndTime);
  pump.SetStopWhenMessagePumpIsIdle(true);
  EXPECT_CALL(delegate, DoWork)
      .Times(2)
      .WillRepeatedly(Return(NextWorkInfo(kNextDelayedWorkTime)));
  EXPECT_CALL(delegate, DoIdleWork).Times(2);

  pump.Run(&delegate);

  EXPECT_THAT(mock_clock.NowTicks(), Eq(kEndTime));
}

TEST(MockMessagePumpTest, StoresNextWakeUpTime) {
  SimpleTestTickClock mock_clock;
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kStartTime = mock_clock.NowTicks();
  const auto kEndTime = kStartTime;
  const auto kNextDelayedWorkTime = kEndTime + Seconds(2);

  pump.SetAllowTimeToAutoAdvanceUntil(kEndTime);
  pump.SetStopWhenMessagePumpIsIdle(true);
  EXPECT_CALL(delegate, DoWork)
      .WillOnce(Return(NextWorkInfo(kNextDelayedWorkTime)));
  EXPECT_CALL(delegate, DoIdleWork);

  pump.Run(&delegate);

  EXPECT_THAT(pump.next_wake_up_time(), Eq(kNextDelayedWorkTime));
}

TEST(MockMessagePumpTest, StoresNextWakeUpTimeInScheduleDelayedWork) {
  SimpleTestTickClock mock_clock;
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kStartTime = mock_clock.NowTicks();
  const auto kNextDelayedWorkTime = kStartTime + Seconds(2);

  pump.ScheduleDelayedWork(MessagePump::Delegate::NextWorkInfo{
      kNextDelayedWorkTime, TimeDelta(), kStartTime});

  EXPECT_THAT(pump.next_wake_up_time(), Eq(kNextDelayedWorkTime));
}

TEST(MockMessagePumpTest, NextDelayedWorkTimeInThePastKeepsRunning) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kNextDelayedWorkTime = mock_clock.NowTicks();
  mock_clock.Advance(Hours(2));

  pump.SetStopWhenMessagePumpIsIdle(true);

  EXPECT_CALL(delegate, DoWork)
      .WillOnce(Return(NextWorkInfo(kNextDelayedWorkTime)))
      .WillOnce(Return(NextWorkInfo(kNextDelayedWorkTime)))
      .WillOnce(Return(NextWorkInfo(TimeTicks::Max())));
  EXPECT_CALL(delegate, DoIdleWork).Times(3);

  pump.Run(&delegate);
}

TEST(MockMessagePumpTest,
     AdvancesUntilAllowedTimeWhenNextDelayedWorkTimeIsMax) {
  SimpleTestTickClock mock_clock;
  mock_clock.Advance(Hours(42));
  StrictMock<MockMessagePumpDelegate> delegate;
  MockTimeMessagePump pump(&mock_clock);
  const auto kAdvanceUntil = mock_clock.NowTicks() + Seconds(123);

  pump.SetStopWhenMessagePumpIsIdle(true);
  pump.SetAllowTimeToAutoAdvanceUntil(kAdvanceUntil);
  EXPECT_CALL(delegate, DoWork)
      .WillRepeatedly(Return(NextWorkInfo(TimeTicks::Max())));
  EXPECT_CALL(delegate, DoIdleWork).Times(2);

  pump.Run(&delegate);

  EXPECT_THAT(mock_clock.NowTicks(), Eq(kAdvanceUntil));
}

}  // namespace
}  // namespace sequence_manager
}  // namespace base
