// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include <type_traits>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#include "base/android/yield_to_looper_checker.h"
#include "base/test/test_support_android.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "base/message_loop/message_pump_default.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Return;

namespace base {

namespace {

// On most platforms, the MessagePump impl controls when native work (e.g.
// handling input messages) gets its turn. Tests below verify that by expecting
// OnBeginWorkItem() calls that cover native work. In some configurations
// however, the platform owns the message loop and is the one yielding to
// Chrome's MessagePump to DoWork(). Under those configurations, it is not
// possible to precisely account for OnBeginWorkItem() calls as they can occur
// nondeterministically. For example, on some versions of iOS, the native loop
// can surprisingly go through multiple cycles of
// kCFRunLoopAfterWaiting=>kCFRunLoopBeforeWaiting before invoking Chrome's
// RunWork() for the first time, triggering multiple  ScopedDoWorkItem 's for
// potential native work before the first DoWork().
constexpr bool ChromeControlsNativeEventProcessing(MessagePumpType pump_type) {
#if BUILDFLAG(IS_MAC)
  return pump_type != MessagePumpType::UI;
#elif BUILDFLAG(IS_IOS)
  return false;
#else
  return true;
#endif
}

class MockMessagePumpDelegate : public MessagePump::Delegate {
 public:
  explicit MockMessagePumpDelegate(MessagePumpType pump_type)
      : check_work_items_(ChromeControlsNativeEventProcessing(pump_type)),
        native_work_item_accounting_is_on_(
            !ChromeControlsNativeEventProcessing(pump_type)) {}

  ~MockMessagePumpDelegate() override { ValidateNoOpenWorkItems(); }

  MockMessagePumpDelegate(const MockMessagePumpDelegate&) = delete;
  MockMessagePumpDelegate& operator=(const MockMessagePumpDelegate&) = delete;

  void BeforeWait() override {}
  void BeginNativeWorkBeforeDoWork() override {}
  MOCK_METHOD(MessagePump::Delegate::NextWorkInfo, DoWork, ());
  MOCK_METHOD(void, DoIdleWork, ());

  // Functions invoked directly by the message pump.
  void OnBeginWorkItem() override {
    any_work_begun_ = true;

    if (check_work_items_) {
      MockOnBeginWorkItem();
    }

    ++work_item_count_;
  }

  void OnEndWorkItem(int run_level_depth) override {
    if (check_work_items_) {
      MockOnEndWorkItem(run_level_depth);
    }

    EXPECT_EQ(run_level_depth, work_item_count_);

    --work_item_count_;

    // It's not possible to close more scopes than there are open ones.
    EXPECT_GE(work_item_count_, 0);
  }

  int RunDepth() override { return work_item_count_; }

  void ValidateNoOpenWorkItems() {
    // Upon exiting there cannot be any open scopes.
    EXPECT_EQ(work_item_count_, 0);

    if (native_work_item_accounting_is_on_) {
// Tests should trigger work beginning at least once except on iOS where
// they need a call to MessagePumpUIApplication::Attach() to do so when on
// the UI thread.
#if !BUILDFLAG(IS_IOS)
      EXPECT_TRUE(any_work_begun_);
#endif
    }
  }

  // Mock functions for asserting.
  MOCK_METHOD(void, MockOnBeginWorkItem, ());
  MOCK_METHOD(void, MockOnEndWorkItem, (int));

  // If native events are covered in the current configuration it's not
  // possible to precisely test all assertions related to work items. This is
  // because a number of speculative WorkItems are created during execution of
  // such loops and it's not possible to determine their number before the
  // execution of the test. In such configurations the functioning of the
  // message pump is still verified by looking at the counts of opened and
  // closed WorkItems.
  const bool check_work_items_;
  const bool native_work_item_accounting_is_on_;

  int work_item_count_ = 0;
  bool any_work_begun_ = false;
};

class MessagePumpTest : public ::testing::TestWithParam<MessagePumpType> {
 public:
  MessagePumpTest() : message_pump_(MessagePump::Create(GetParam())) {}

 protected:
#if defined(USE_GLIB)
  // Because of a GLIB implementation quirk, the pump doesn't do the same things
  // between each DoWork. In this case, it won't set/clear a ScopedDoWorkItem
  // because we run a chrome work item in the runloop outside of GLIB's control,
  // so we oscillate between setting and not setting PreDoWorkExpectations.
  std::map<MessagePump::Delegate*, int> do_work_counts;
#endif
  void AddPreDoWorkExpectations(
      testing::StrictMock<MockMessagePumpDelegate>& delegate) {
#if BUILDFLAG(IS_WIN)
    if (GetParam() == MessagePumpType::UI) {
      // The Windows MessagePumpForUI may do native work from ::PeekMessage()
      // and labels itself as such.
      EXPECT_CALL(delegate, MockOnBeginWorkItem);
      EXPECT_CALL(delegate, MockOnEndWorkItem);

      // If the above event was MessagePumpForUI's own kMsgHaveWork internal
      // event, it will process another event to replace it (ref.
      // ProcessPumpReplacementMessage).
      EXPECT_CALL(delegate, MockOnBeginWorkItem).Times(AtMost(1));
      EXPECT_CALL(delegate, MockOnEndWorkItem).Times(AtMost(1));
    }
#endif  // BUILDFLAG(IS_WIN)
#if defined(USE_GLIB)
    do_work_counts.try_emplace(&delegate, 0);
    if (GetParam() == MessagePumpType::UI) {
      if (++do_work_counts[&delegate] % 2) {
        // The GLib MessagePump will do native work before chrome work on
        // startup.
        EXPECT_CALL(delegate, MockOnBeginWorkItem);
        EXPECT_CALL(delegate, MockOnEndWorkItem);
      }
    }
#endif  // defined(USE_GLIB)
  }

  void AddPostDoWorkExpectations(
      testing::StrictMock<MockMessagePumpDelegate>& delegate) {
#if defined(USE_GLIB)
    if (GetParam() == MessagePumpType::UI) {
      // The GLib MessagePump can create and destroy work items between DoWorks
      // depending on internal state.
      EXPECT_CALL(delegate, MockOnBeginWorkItem).Times(AtMost(1));
      EXPECT_CALL(delegate, MockOnEndWorkItem).Times(AtMost(1));
    }
#endif  // defined(USE_GLIB)
  }

  std::unique_ptr<MessagePump> message_pump_;
};

}  // namespace

TEST(MessagePumpTest, NextWorkInfoRemainingDelay) {
  MessagePump::Delegate::NextWorkInfo info;
  info.recent_now = TimeTicks::Now();

  info.delayed_run_time = TimeTicks::Max();
  EXPECT_EQ(info.remaining_delay(), TimeDelta::Max());

  info.delayed_run_time = info.recent_now + Milliseconds(10);
  EXPECT_EQ(info.remaining_delay(), Milliseconds(10));

  info.recent_now -= Milliseconds(5);
  EXPECT_EQ(info.remaining_delay(), Milliseconds(15));
}

TEST_P(MessagePumpTest, QuitStopsWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  // Not expecting any calls to DoIdleWork after quitting, nor any of the
  // PostDoWorkExpectations, quitting should be instantaneous.
  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  // MessagePumpGlib uses a work item between a HandleDispatch() call and
  // passing control back to the chrome loop, which handles the Quit() despite
  // us not necessarily doing any native work during that time.
#if defined(USE_GLIB)
  if (GetParam() == MessagePumpType::UI) {
    AddPostDoWorkExpectations(delegate);
  }
#endif

  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

#if BUILDFLAG(IS_ANDROID)
class MockInputHintChecker : public android::InputHintChecker {
 public:
  MOCK_METHOD(bool, HasInputImplWithThrottling, (), (override));
};

TEST_P(MessagePumpTest, DetectingHasInputYieldsOnUi) {
  testing::InSequence sequence;
  MessagePumpType pump_type = GetParam();
  testing::StrictMock<MockMessagePumpDelegate> delegate(pump_type);
  testing::StrictMock<MockInputHintChecker> hint_checker_mock;
  android::InputHintChecker::ScopedOverrideInstance scoped_override_hint(
      &hint_checker_mock);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(android::kYieldWithInputHint);
  android::InputHintChecker::InitializeFeatures();
  uint32_t initial_work_enters = GetAndroidNonDelayedWorkEnterCount();

  // Override the first DoWork() to return an immediate next.
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    auto work_info =
        MessagePump::Delegate::NextWorkInfo{.delayed_run_time = TimeTicks()};
    CHECK(work_info.is_immediate());
    return work_info;
  });

  if (pump_type == MessagePumpType::UI) {
    // Override the following InputHintChecker::HasInput() to return true.
    EXPECT_CALL(hint_checker_mock, HasInputImplWithThrottling()).WillOnce([] {
      return true;
    });
  }

  // Override the second DoWork() to quit the loop.
  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{.delayed_run_time =
                                                   TimeTicks::Max()};
  });

  // No immediate next_work_info remaining before the yield. Not expecting
  // to observe an input hint check.
  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->Run(&delegate);

  // Expect two calls to DoNonDelayedLooperWork(). The first one occurs as a
  // result of MessagePump::Run(). The second one is the result of yielding
  // after HasInput() returns true. For non-UI MessagePumpType the
  // MessagePump::Create() does not intercept entering DoNonDelayedLooperWork(),
  // so it remains 0 instead of 1.
  uint32_t work_loop_entered = (pump_type == MessagePumpType::UI) ? 2 : 0;
  EXPECT_EQ(initial_work_enters + work_loop_entered,
            GetAndroidNonDelayedWorkEnterCount());
}

TEST_P(MessagePumpTest, YieldDuringStartup) {
  testing::InSequence sequence;
  MessagePumpType pump_type = GetParam();
  testing::StrictMock<MockMessagePumpDelegate> delegate(pump_type);

  uint32_t initial_work_enters = GetAndroidNonDelayedWorkEnterCount();

  // Override the first DoWork() to return an immediate next. Also set startup
  // as running.
  EXPECT_CALL(delegate, DoWork).WillOnce([pump_type] {
    if (pump_type == MessagePumpType::UI) {
      android::YieldToLooperChecker::GetInstance().SetStartupRunning(true);
    }
    auto work_info =
        MessagePump::Delegate::NextWorkInfo{.delayed_run_time = TimeTicks()};
    CHECK(work_info.is_immediate());
    return work_info;
  });

  // Override the second DoWork() and mark startup as complete so we don't yield
  // again.
  EXPECT_CALL(delegate, DoWork).WillOnce([pump_type] {
    if (pump_type == MessagePumpType::UI) {
      // Mark startup as done so we don't yield again
      android::YieldToLooperChecker::GetInstance().SetStartupRunning(false);
    }
    return MessagePump::Delegate::NextWorkInfo{.delayed_run_time = TimeTicks()};
  });

  // Override the third DoWork() to quit the loop.
  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{.delayed_run_time =
                                                   TimeTicks::Max()};
  });

  // No immediate next_work_info remaining before the yield.
  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->Run(&delegate);

  // Expect two calls to DoNonDelayedLooperWork(). The first one occurs as a
  // result of MessagePump::Run(). The second one is the result of yielding
  // after YieldDuringStartup() returns true. For non-UI MessagePumpType the
  // MessagePump::Create() does not intercept entering DoNonDelayedLooperWork(),
  // so it remains 0 instead of 1.
  uint32_t work_loop_entered = (pump_type == MessagePumpType::UI) ? 2 : 0;
  EXPECT_EQ(initial_work_enters + work_loop_entered,
            GetAndroidNonDelayedWorkEnterCount());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_P(MessagePumpTest, QuitStopsWorkWithNestedRunLoop) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  // We first schedule a call to DoWork, which runs a nested run loop. After
  // the nested loop exits, we schedule another DoWork which quits the outer
  // (original) run loop. The test verifies that there are no extra calls to
  // DoWork after the outer loop quits.
  EXPECT_CALL(delegate, DoWork).WillOnce([&] {
    message_pump_->Run(&nested_delegate);
    // A null NextWorkInfo indicates immediate follow-up work.
    return MessagePump::Delegate::NextWorkInfo();
  });

  AddPreDoWorkExpectations(nested_delegate);
  EXPECT_CALL(nested_delegate, DoWork).WillOnce([&] {
    // Quit the nested run loop.
    message_pump_->Quit();
    // The underlying pump should process the next task in the first run-level
    // regardless of whether the nested run-level indicates there's no more work
    // (e.g. can happen when the only remaining tasks are non-nestable).
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  // The `nested_delegate` will quit first.
  AddPostDoWorkExpectations(nested_delegate);

  // Return a delayed task with |yield_to_native| set, and exit.
  AddPostDoWorkExpectations(delegate);

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, LeewaySmokeTest) {
  // The handling of the "leeway" in the NextWorkInfo is only implemented on
  // mac. However since we inject a fake one for testing this is hard to test.
  // This test ensures that setting this boolean doesn't cause any MessagePump
  // to explode.
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  testing::InSequence sequence;

  AddPreDoWorkExpectations(delegate);
  // Return a delayed task with |yield_to_native| set, and exit.
  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    auto now = TimeTicks::Now();
    return MessagePump::Delegate::NextWorkInfo{now + Milliseconds(1),
                                               Milliseconds(8), now};
  });
  EXPECT_CALL(delegate, DoIdleWork()).Times(AnyNumber());

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, RunWithoutScheduleWorkInvokesDoWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  AddPostDoWorkExpectations(delegate);

#if BUILDFLAG(IS_IOS)
  EXPECT_CALL(delegate, DoIdleWork).Times(AnyNumber());
#endif

  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, NestedRunWithoutScheduleWorkInvokesDoWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce([this, &nested_delegate] {
    message_pump_->Run(&nested_delegate);
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  AddPreDoWorkExpectations(nested_delegate);

  EXPECT_CALL(nested_delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  // We quit `nested_delegate` before `delegate`
  AddPostDoWorkExpectations(nested_delegate);

  AddPostDoWorkExpectations(delegate);

#if BUILDFLAG(IS_IOS)
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

// On iOS, MessagePumpDefault is not used.
#if !BUILDFLAG(IS_IOS)

class MessagePumpDefaultTest : public ::testing::Test {
 protected:
  void VerifyBusyWaitHistogramExpectations(bool task_arrived) {
    histogram_tester_.ExpectTotalCount(
        "Scheduling.MessagePumpDefault.BusyLoop.Duration.TimedOut",
        task_arrived ? 0 : 1);
    histogram_tester_.ExpectTotalCount(
        "Scheduling.MessagePumpDefault.BusyLoop.Duration.TaskArrived",
        task_arrived ? 1 : 0);
    histogram_tester_.ExpectBucketCount(
        "Scheduling.MessagePumpDefault.BusyLoop.TaskArrived", task_arrived, 1);
    histogram_tester_.ExpectTotalCount(
        "Scheduling.MessagePumpDefault.BusyLoop.TargetDuration", 1);
  }

  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
  base::HistogramTester histogram_tester_;
  MessagePumpDefault message_pump_;
};

TEST_F(MessagePumpDefaultTest, BusyWaitOnEventRespectsNextWorkDelay) {
  // Very large, to make the test less flaky.
  TimeDelta max_busy_loop_time = Milliseconds(100);
  message_pump_.SetBusyLoop(max_busy_loop_time);

  TimeTicks before = TimeTicks::Now();
  // `next_work_delay` is smaller than `max_busy_loop_time`, so we expect to
  // busy loop for next_work_delay and not `max_busy_loop_time`.
  TimeDelta next_work_delay = Milliseconds(2);

  bool signaled = message_pump_.BusyWaitOnEvent(before, next_work_delay);
  ASSERT_FALSE(signaled);
  TimeDelta busy_loop_duration = TimeTicks::Now() - before;
  EXPECT_LT(busy_loop_duration, max_busy_loop_time);
  EXPECT_GE(busy_loop_duration, next_work_delay);

  VerifyBusyWaitHistogramExpectations(/*task_arrived=*/false);
}

TEST_F(MessagePumpDefaultTest, BusyWaitOnEventDoesNotLoopForMoreThanRequired) {
  TimeDelta max_busy_loop_time = Milliseconds(2);
  message_pump_.SetBusyLoop(max_busy_loop_time);

  TimeTicks before = TimeTicks::Now();
  TimeDelta next_work_delay = Milliseconds(100);

  bool signaled = message_pump_.BusyWaitOnEvent(before, next_work_delay);
  ASSERT_FALSE(signaled);
  TimeDelta busy_loop_duration = TimeTicks::Now() - before;
  EXPECT_GE(busy_loop_duration, max_busy_loop_time);
  EXPECT_LT(busy_loop_duration, next_work_delay);

  VerifyBusyWaitHistogramExpectations(/*task_arrived=*/false);
}

TEST_F(MessagePumpDefaultTest, BusyWaitOnEventStopsIfSignaled) {
  TimeDelta max_busy_loop_time = Milliseconds(50);
  message_pump_.SetBusyLoop(max_busy_loop_time);

  TimeTicks before = TimeTicks::Now();
  TimeDelta next_work_delay = Milliseconds(100);

  message_pump_.ScheduleWork();
  bool signaled = message_pump_.BusyWaitOnEvent(before, next_work_delay);
  EXPECT_TRUE(signaled);
  // Could expect it to be smaller, since it should return immediately, but this
  // is to avoid flakiness.
  EXPECT_LT(TimeTicks::Now() - before, max_busy_loop_time);

  VerifyBusyWaitHistogramExpectations(/*task_arrived=*/true);
}

TEST_F(MessagePumpDefaultTest, ShouldBusyLoopHeuristic) {
  EXPECT_FALSE(message_pump_.ShouldBusyLoop());

  base::TimeDelta busy_loop_for = base::Milliseconds(1);
  message_pump_.SetBusyLoop(busy_loop_for);
  EXPECT_TRUE(message_pump_.ShouldBusyLoop());

  // Many long waits, no more busy looping.
  for (int i = 0; i < 10; i++) {
    message_pump_.RecordWaitTime(busy_loop_for * 10);
  }
  EXPECT_FALSE(message_pump_.ShouldBusyLoop());

  // One short wait, busy loop.
  message_pump_.RecordWaitTime(busy_loop_for / 1.5);
  EXPECT_TRUE(message_pump_.ShouldBusyLoop());
  // But as long as the moving average is high enough, don't loop.
  message_pump_.RecordWaitTime(busy_loop_for * 1.5);
  EXPECT_FALSE(message_pump_.ShouldBusyLoop());

  // Eventually, the moving average gets low enough
  for (int i = 0; i < 100; i++) {
    message_pump_.RecordWaitTime(busy_loop_for / 10);
  }
  EXPECT_TRUE(message_pump_.ShouldBusyLoop());

  // Even if the last wait time was higher than the limit.
  message_pump_.RecordWaitTime(busy_loop_for * 1.5);
  EXPECT_TRUE(message_pump_.ShouldBusyLoop());
}

TEST_F(MessagePumpDefaultTest, BusyLoopPredictionAccuracyHistogram) {
  constexpr base::TimeDelta kBusyLoopMaxDuration = base::Milliseconds(1);
  constexpr std::string_view kHistogramName =
      "Scheduling.MessagePumpDefault.BusyLoop.PredictionAccuracy";

  testing::StrictMock<MockMessagePumpDelegate> delegate(
      MessagePumpType::DEFAULT);
  testing::InSequence sequence;

  auto run_message_pump = [&](bool task_arrived) {
    EXPECT_CALL(delegate, DoWork).WillOnce([&] {
      if (task_arrived) {
        message_pump_.ScheduleWork();
      }
      // Schedule a delayed task to arrive after the busy loop max duration.
      auto now = base::TimeTicks::Now();
      return MessagePump::Delegate::NextWorkInfo{
          .delayed_run_time = now + kBusyLoopMaxDuration * 10,
          .recent_now = now};
    });
    EXPECT_CALL(delegate, DoIdleWork);
    EXPECT_CALL(delegate, DoWork).WillOnce([&] {
      message_pump_.Quit();
      return MessagePump::Delegate::NextWorkInfo{base::TimeTicks::Max()};
    });
    message_pump_.Run(&delegate);
  };

  // Update the moving average to be above or below the limit.
  auto modify_moving_average = [&](bool lower) {
    for (int i = 0; i < 100; i++) {
      message_pump_.RecordWaitTime(lower ? kBusyLoopMaxDuration / 2
                                         : kBusyLoopMaxDuration * 2);
    }
  };

  message_pump_.SetBusyLoop(kBusyLoopMaxDuration);

  // 1. True Positive: Heuristic says busy loop and a task arrived within the
  //    max busy loop duration.
  modify_moving_average(/*lower=*/true);
  ASSERT_TRUE(message_pump_.ShouldBusyLoop());
  run_message_pump(/*task_arrived=*/true);
  histogram_tester_.ExpectTotalCount(kHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kHistogramName, 1 /*kTruePositive*/, 1);

  // 2. False Positive: Heuristic says busy loop but no task arrived within the
  //    max busy loop duration.
  modify_moving_average(/*lower=*/true);
  ASSERT_TRUE(message_pump_.ShouldBusyLoop());
  run_message_pump(/*task_arrived=*/false);
  histogram_tester_.ExpectTotalCount(kHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kHistogramName, 0 /*kFalsePositive*/, 1);

  // 3. True Negative: Heuristic says don't busy loop and no task arrived within
  //    the max busy loop duration.
  modify_moving_average(/*lower=*/false);
  ASSERT_FALSE(message_pump_.ShouldBusyLoop());
  run_message_pump(/*task_arrived=*/false);
  histogram_tester_.ExpectTotalCount(kHistogramName, 3);
  histogram_tester_.ExpectBucketCount(kHistogramName, 3 /*kTrueNegative*/, 1);

  // 4. False Negative: Heuristic says don't busy loop but a task arrived within
  //    the max busy loop duration.
  modify_moving_average(/*lower=*/false);
  ASSERT_FALSE(message_pump_.ShouldBusyLoop());
  run_message_pump(/*task_arrived=*/true);
  histogram_tester_.ExpectTotalCount(kHistogramName, 4);
  histogram_tester_.ExpectBucketCount(kHistogramName, 2 /*kFalseNegative*/, 1);
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace base
