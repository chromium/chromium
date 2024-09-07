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
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#include "base/test/test_support_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Invoke;
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
  MOCK_METHOD0(DoWork, MessagePump::Delegate::NextWorkInfo());
  MOCK_METHOD0(DoIdleWork, void());

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
  MOCK_METHOD0(MockOnBeginWorkItem, void(void));
  MOCK_METHOD1(MockOnEndWorkItem, void(int));

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

TEST_P(MessagePumpTest, QuitStopsWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  // Not expecting any calls to DoIdleWork after quitting, nor any of the
  // PostDoWorkExpectations, quitting should be instantaneous.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

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
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([] {
    auto work_info =
        MessagePump::Delegate::NextWorkInfo{.delayed_run_time = TimeTicks()};
    CHECK(work_info.is_immediate());
    return work_info;
  }));

  if (pump_type == MessagePumpType::UI) {
    // Override the following InputHintChecker::HasInput() to return true.
    EXPECT_CALL(hint_checker_mock, HasInputImplWithThrottling())
        .WillOnce(Invoke([] { return true; }));
  }

  // Override the second DoWork() to quit the loop.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{.delayed_run_time =
                                                   TimeTicks::Max()};
  }));

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
#endif

TEST_P(MessagePumpTest, QuitStopsWorkWithNestedRunLoop) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());
  testing::StrictMock<MockMessagePumpDelegate> nested_delegate(GetParam());

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

  // The `nested_delegate` will quit first.
  AddPostDoWorkExpectations(nested_delegate);

  // Return a delayed task with |yield_to_native| set, and exit.
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
  // implemented on the MessagePumpAndroid. However since we inject a fake one
  // for testing this is hard to test. This test ensures that setting this
  // boolean doesn't cause any MessagePump to explode.
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  testing::InSequence sequence;

  // Return an immediate task with |yield_to_native| set.
  AddPreDoWorkExpectations(delegate);
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks(), TimeDelta(),
                                               TimeTicks(),
                                               /* yield_to_native = */ true};
  }));
  AddPostDoWorkExpectations(delegate);

  AddPreDoWorkExpectations(delegate);
  // Return a delayed task with |yield_to_native| set, and exit.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    auto now = TimeTicks::Now();
    return MessagePump::Delegate::NextWorkInfo{now + Milliseconds(1),
                                               TimeDelta(), now, true};
  }));
  EXPECT_CALL(delegate, DoIdleWork()).Times(AnyNumber());

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
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    auto now = TimeTicks::Now();
    return MessagePump::Delegate::NextWorkInfo{now + Milliseconds(1),
                                               Milliseconds(8), now};
  }));
  EXPECT_CALL(delegate, DoIdleWork()).Times(AnyNumber());

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

TEST_P(MessagePumpTest, RunWithoutScheduleWorkInvokesDoWork) {
  testing::InSequence sequence;
  testing::StrictMock<MockMessagePumpDelegate> delegate(GetParam());

  AddPreDoWorkExpectations(delegate);

  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  }));

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

}  // namespace base
