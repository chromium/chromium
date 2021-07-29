// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include <type_traits>

#include "base/bind.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
#include "base/message_loop/message_pump_libevent.h"
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;

namespace base {

namespace {

class MockMessagePumpDelegate : public MessagePump::Delegate {
 public:
  MockMessagePumpDelegate() = default;

  // MessagePump::Delegate:
  void BeforeWait() override {}
  MOCK_METHOD0(DoWork, MessagePump::Delegate::NextWorkInfo());
  MOCK_METHOD0(DoIdleWork, bool());

  MOCK_METHOD0(OnBeginWorkItem, void(void));
  MOCK_METHOD0(OnEndWorkItem, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessagePumpDelegate);
};

class MessagePumpTest : public ::testing::TestWithParam<MessagePumpType> {
 public:
  MessagePumpTest() : message_pump_(MessagePump::Create(GetParam())) {}

 protected:
  void AddPreDoWorkExpectations(
      testing::StrictMock<MockMessagePumpDelegate>& delegate) {
#if defined(OS_WIN)
    if (GetParam() == MessagePumpType::UI) {
      // The Windows MessagePumpForUI may do native work from ::PeekMessage()
      // and labels itself as such.
      EXPECT_CALL(delegate, OnBeginWorkItem);
      EXPECT_CALL(delegate, OnEndWorkItem);

      // If the above event was MessagePumpForUI's own kMsgHaveWork internal
      // event, it will process another event to replace it (ref.
      // ProcessPumpReplacementMessage).
      EXPECT_CALL(delegate, OnBeginWorkItem).Times(AtMost(1));
      EXPECT_CALL(delegate, OnEndWorkItem).Times(AtMost(1));
    }
#endif  // defined(OS_WIN)
  }

  void AddPostDoWorkExpectations(
      testing::StrictMock<MockMessagePumpDelegate>& delegate) {
#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
    if ((GetParam() == MessagePumpType::UI &&
         std::is_same<MessagePumpForUI, MessagePumpLibevent>::value) ||
        (GetParam() == MessagePumpType::IO &&
         std::is_same<MessagePumpForIO, MessagePumpLibevent>::value)) {
      // MessagePumpLibEvent checks for native notifications once after
      // processing a DoWork().
      EXPECT_CALL(delegate, OnBeginWorkItem);
      EXPECT_CALL(delegate, OnEndWorkItem);
    }
#endif  // defined(OS_POSIX) && !defined(OS_NACL_SFI)
  }

  std::unique_ptr<MessagePump> message_pump_;
};

}  // namespace

TEST_P(MessagePumpTest, QuitStopsWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;

  AddPreDoWorkExpectations(delegate);

  // Not expecting any calls to DoIdleWork after quitting, nor any of the
  // PostDoWorkExpectations, quitting should be instantaneous.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));
  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, QuitStopsWorkWithNestedRunLoop) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate;

  AddPreDoWorkExpectations(delegate);

  // We first schedule a call to DoWork, which runs a nested run loop. After
  // the nested loop exits, we schedule another DoWork which quits the outer
  // (original) run loop. The test verifies that there are no extra calls to
  // DoWork after the outer loop quits.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([&] {
    message_pump_->Run(&nested_delegate);
    // A null NextWorkInfo indicates immediate follow-up work.
    return MessagePump::Delegate::NextWorkInfo();
  }));

  AddPreDoWorkExpectations(nested_delegate);
  EXPECT_CALL(nested_delegate, DoWork).WillOnce(Invoke([&] {
    // Quit the nested run loop.
    message_pump_->Quit();
    // The underlying pump should process the next task in the first run-level
    // regardless of whether the nested run-level indicates there's no more work
    // (e.g. can happen when the only remaining tasks are non-nestable).
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

  // PostDoWorkExpectations for the first DoWork.
  AddPostDoWorkExpectations(delegate);

  AddPreDoWorkExpectations(delegate);
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, YieldToNativeRequestedSmokeTest) {
  // The handling of the "yield_to_native" boolean in the NextWorkInfo is only
  // implemented on the MessagePumpForUI on android. However since we inject a
  // fake one for testing this is hard to test. This test ensures that setting
  // this boolean doesn't cause any MessagePump to explode.
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;

  // Return an immediate task with |yield_to_native| set.
  AddPreDoWorkExpectations(delegate);
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks(), TimeTicks(),
                                               /* yield_to_native = */ true};
  }));
  AddPostDoWorkExpectations(delegate);

  // Return a delayed task with |yield_to_native| set, and exit.
  AddPreDoWorkExpectations(delegate);
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    auto now = TimeTicks::Now();
    return MessagePump::Delegate::NextWorkInfo{
        now + TimeDelta::FromMilliseconds(1), now, true};
  }));
  EXPECT_CALL(delegate, DoIdleWork()).Times(AnyNumber());

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

namespace {

class TimerSlackTestDelegate : public MessagePump::Delegate {
 public:
  TimerSlackTestDelegate(MessagePump* message_pump)
      : message_pump_(message_pump) {
    // We first schedule a delayed task far in the future with maximum timer
    // slack.
    message_pump_->SetTimerSlack(TIMER_SLACK_MAXIMUM);
    message_pump_->ScheduleDelayedWork(TimeTicks::Now() +
                                       TimeDelta::FromHours(1));

    // Since we have no other work pending, the pump will initially be idle.
    action_.store(NONE);
  }

  void OnBeginWorkItem() override {}
  void OnEndWorkItem() override {}
  void BeforeWait() override {}

  MessagePump::Delegate::NextWorkInfo DoWork() override {
    switch (action_.load()) {
      case NONE:
        break;
      case SCHEDULE_DELAYED_WORK: {
        // After being woken up by the other thread, we let the pump know that
        // the next delayed task is in fact much sooner than the 1 hour delay it
        // was aware of. If the pump refreshes its timer correctly, it will wake
        // up shortly, finishing the test.
        action_.store(QUIT);
        TimeTicks now = TimeTicks::Now();
        return {now + TimeDelta::FromMilliseconds(50), now};
      }
      case QUIT:
        message_pump_->Quit();
        break;
    }
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }

  bool DoIdleWork() override { return false; }

  void WakeUpFromOtherThread() {
    action_.store(SCHEDULE_DELAYED_WORK);
    message_pump_->ScheduleWork();
  }

 private:
  enum Action {
    NONE,
    SCHEDULE_DELAYED_WORK,
    QUIT,
  };

  MessagePump* const message_pump_;
  std::atomic<Action> action_;
};

}  // namespace

TEST_P(MessagePumpTest, TimerSlackWithLongDelays) {
  // This is a regression test for an issue where the iOS message pump fails to
  // run delayed work when timer slack is enabled. The steps needed to trigger
  // this are:
  //
  //  1. The message pump timer slack is set to maximum.
  //  2. A delayed task is posted for far in the future (e.g., 1h).
  //  3. The system goes idle at least for a few seconds.
  //  4. Another delayed task is posted with a much smaller delay.
  //
  // The following message pump test delegate automatically runs through this
  // sequence.
  TimerSlackTestDelegate delegate(message_pump_.get());

  // We use another thread to wake up the pump after 2 seconds to allow the
  // system to enter an idle state. This delay was determined experimentally on
  // the iPhone 6S simulator.
  Thread thread("Waking thread");
  thread.StartAndWaitForTesting();
  thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindLambdaForTesting([&delegate] { delegate.WakeUpFromOtherThread(); }),
      TimeDelta::FromSeconds(2));

  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, RunWithoutScheduleWorkInvokesDoWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));
#if defined(OS_IOS)
  EXPECT_CALL(delegate, DoIdleWork).Times(AnyNumber());
#endif
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, NestedRunWithoutScheduleWorkInvokesDoWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate;
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate;

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this, &nested_delegate] {
    message_pump_->Run(&nested_delegate);
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

  AddPreDoWorkExpectations(nested_delegate);

  EXPECT_CALL(nested_delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

#if defined(OS_IOS)
  EXPECT_CALL(nested_delegate, DoIdleWork).Times(AnyNumber());
  EXPECT_CALL(delegate, DoIdleWork).Times(AnyNumber());
#endif

  message_pump_->Run(&delegate);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MessagePumpTest,
                         ::testing::Values(MessagePumpType::DEFAULT,
                                           MessagePumpType::UI,
                                           MessagePumpType::IO));

#if defined(OS_WIN)

TEST(MessagePumpTestWin, WmQuitIsNotIgnoredWithEnableWmQuit) {
  SingleThreadTaskExecutor task_executor(
      MessagePumpType::UI_WITH_WM_QUIT_SUPPORT);

  // Post a WM_QUIT message to the current thread.
  ::PostQuitMessage(0);

  // Post a task to the current thread, with a small delay to make it less
  // likely that we process the posted task before looking for WM_* messages.
  RunLoop run_loop;
  task_executor.task_runner()->PostDelayedTask(FROM_HERE,
                                               BindOnce(
                                                   [](OnceClosure closure) {
                                                     ADD_FAILURE();
                                                     std::move(closure).Run();
                                                   },
                                                   run_loop.QuitClosure()),
                                               TestTimeouts::tiny_timeout());

  // Run the loop. It should not result in ADD_FAILURE() getting called.
  run_loop.Run();
}

#endif  // defined(OS_WIN)

}  // namespace base
