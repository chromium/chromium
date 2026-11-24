// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/message_pump_type.h"
#include "base/message_loop/message_pump_wakeup_counter.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#include "base/android/yield_to_looper_checker.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_support_android.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "base/message_loop/message_pump_default.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/message_loop/message_pump_for_io_event_win.h"
#include "base/message_loop/message_pump_win.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_message_pump_for_io_factory_win.h"
#include "base/win/scoped_handle.h"
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

#if BUILDFLAG(IS_WIN)
class MockNativeWorkMessagePumpForIO
    : public MessagePumpForIOCompletionPortImpl {
 public:
  MOCK_METHOD(bool, ScheduleNativeWork, (), (override));
};

class MockIdlePollMessagePumpForIO : public MessagePumpForIOCompletionPortImpl {
 public:
  MOCK_METHOD(bool, PollForIOBeforeIdle, (), (override));
};

win::ScopedHandle CreateIOEventForTesting() {
  win::ScopedHandle event(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
  PCHECK(event.is_valid());
  return event;
}

class CallbackIOEventHandler : public MessagePumpForIO::IOEventHandler {
 public:
  explicit CallbackIOEventHandler(RepeatingClosure callback,
                                  bool stop_watching = true)
      : IOEventHandler(FROM_HERE),
        callback_(std::move(callback)),
        stop_watching_(stop_watching) {}

  void OnIOEventSignaled(
      MessagePumpForIO::IOEventWatchController* controller) override {
    if (stop_watching_) {
      controller->StopWatching();
    }
    callback_.Run();
  }

 private:
  RepeatingClosure callback_;
  const bool stop_watching_;
};

std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
WatchIOEventsForTesting(size_t count,
                        MessagePumpForIO::IOEventHandler* handler,
                        MessagePumpForIO* pump = nullptr) {
  std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
      watches;
  watches.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    watches.push_back(
        std::make_unique<MessagePumpForIO::IOEventWatchController>(
            CreateIOEventForTesting()));
    if (pump) {
      CHECK(pump->WatchIOEvent(watches.back().get(), handler));
    } else {
      CHECK(
          CurrentIOThread::Get()->WatchIOEvent(watches.back().get(), handler));
    }
  }
  return watches;
}

// Waits until `WaitShard` workers have queued at least `count` completions.
// Polls the queue without running the pump, so the completions stay
// undispatched. Fails, rather than hanging, if they aren't queued within
// `TestTimeouts::action_timeout()`.
[[nodiscard]] testing::AssertionResult WaitForQueuedShardCompletions(
    const MessagePumpForIOEventImpl& pump,
    size_t count = 1) {
  const TimeTicks deadline = TimeTicks::Now() + TestTimeouts::action_timeout();
  while (pump.GetPendingShardCompletionCountForTesting() < count) {
    if (TimeTicks::Now() >= deadline) {
      return testing::AssertionFailure()
             << "Timed out waiting for " << count
             << " queued shard completion(s); have "
             << pump.GetPendingShardCompletionCountForTesting();
    }
    PlatformThread::YieldCurrentThread();
  }
  return testing::AssertionSuccess();
}
#endif

}  // namespace

#if BUILDFLAG(IS_WIN)
TEST(MessagePumpForIOTest, FailedWakeupAllowsRetry) {
  testing::StrictMock<MockNativeWorkMessagePumpForIO> pump;
  testing::InSequence sequence;
  EXPECT_CALL(pump, ScheduleNativeWork()).WillOnce(testing::Return(false));
  EXPECT_CALL(pump, ScheduleNativeWork()).WillOnce(testing::Return(true));

  pump.ScheduleWork();
  pump.ScheduleWork();
  pump.ScheduleWork();
}

// PollForIOBeforeIdle() is skipped while DoWork() reports immediate work, and
// a successful poll prevents DoIdleWork().
TEST(MessagePumpForIOTest, PollsForIOOnlyBeforeIdle) {
  testing::StrictMock<MockIdlePollMessagePumpForIO> pump;
  testing::NiceMock<MockMessagePumpDelegate> delegate(MessagePumpType::IO);
  testing::InSequence sequence;
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    return MessagePump::Delegate::NextWorkInfo();
  });
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(pump, PollForIOBeforeIdle).WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(pump, PollForIOBeforeIdle).WillOnce(testing::Return(false));
  EXPECT_CALL(delegate, DoIdleWork).WillOnce([&] { pump.Quit(); });
  pump.Run(&delegate);
}

TEST(MessagePumpForIOEventTest, ScopedOverrideRestoresProductionBackend) {
  EXPECT_FALSE(MessagePumpForIO::Create()->SupportsIOEvents());
  {
    test::ScopedMessagePumpForIOEventFactoryForTesting factory;
    EXPECT_TRUE(MessagePumpForIO::Create()->SupportsIOEvents());
  }
  EXPECT_FALSE(MessagePumpForIO::Create()->SupportsIOEvents());
}

TEST(MessagePumpForIOEventTest, NestedScopedOverridesRestorePreviousValue) {
  {
    test::ScopedMessagePumpForIOEventFactoryForTesting outer;
    {
      test::ScopedMessagePumpForIOEventFactoryForTesting inner;
      EXPECT_TRUE(MessagePumpForIO::Create()->SupportsIOEvents());
    }
    EXPECT_TRUE(MessagePumpForIO::Create()->SupportsIOEvents());
  }
  EXPECT_FALSE(MessagePumpForIO::Create()->SupportsIOEvents());
}

TEST(MessagePumpForIOEventTest, DispatchesAndStopsWatch) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  ASSERT_TRUE(CurrentIOThread::Get()->SupportsIOEvents());
  RunLoop run_loop;
  int notifications = 0;
  CallbackIOEventHandler handler(BindLambdaForTesting([&] {
    ++notifications;
    run_loop.Quit();
  }));
  auto event = CreateIOEventForTesting();
  const HANDLE event_handle = event.get();
  MessagePumpForIO::IOEventWatchController controller(std::move(event));
  EXPECT_FALSE(event.is_valid());
  ASSERT_EQ(controller.event_handle(), event_handle);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&controller, &handler));
  ASSERT_TRUE(::SetEvent(event_handle));
  run_loop.Run();
  controller.StopWatching();

  // An unwatched signaled event must not dispatch again. A later-registered
  // watched event quits the loop only after native I/O has been polled.
  RunLoop poll_loop;
  CallbackIOEventHandler poll_handler(poll_loop.QuitClosure());
  MessagePumpForIO::IOEventWatchController poll_controller(
      CreateIOEventForTesting());
  ASSERT_TRUE(
      CurrentIOThread::Get()->WatchIOEvent(&poll_controller, &poll_handler));
  ASSERT_TRUE(::SetEvent(controller.event_handle()));
  ASSERT_TRUE(::SetEvent(poll_controller.event_handle()));
  poll_loop.Run();
  EXPECT_EQ(notifications, 1);
}

TEST(MessagePumpForIOEventTest, PumpTeardownDetachesController) {
  CallbackIOEventHandler handler(BindRepeating([] { ADD_FAILURE(); }));
  MessagePumpForIO::IOEventWatchController controller(
      CreateIOEventForTesting());
  {
    MessagePumpForIOEventImpl pump;
    ASSERT_TRUE(pump.WatchIOEvent(&controller, &handler));
  }
  EXPECT_TRUE(::SetEvent(controller.event_handle()));
  controller.StopWatching();
}

TEST(MessagePumpForIOEventTest, DispatchesAcrossMultipleShardsAndRearms) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  RunLoop run_loop;
  constexpr size_t kWatchCount = 3 * MAXIMUM_WAIT_OBJECTS;
  std::vector<std::unique_ptr<CallbackIOEventHandler>> handlers;
  handlers.reserve(kWatchCount);
  std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
      watches;
  watches.reserve(kWatchCount);
  std::vector<int> notifications(kWatchCount);
  size_t completed = 0;
  for (size_t i = 0; i < kWatchCount; ++i) {
    watches.push_back(
        std::make_unique<MessagePumpForIO::IOEventWatchController>(
            CreateIOEventForTesting()));
    handlers.push_back(std::make_unique<CallbackIOEventHandler>(
        BindLambdaForTesting([&, i] {
          if (++notifications[i] == 1) {
            EXPECT_TRUE(::SetEvent(watches[i]->event_handle()));
          } else {
            EXPECT_EQ(notifications[i], 2);
            watches[i]->StopWatching();
            if (++completed == kWatchCount) {
              run_loop.Quit();
            }
          }
        }),
        /*stop_watching=*/false));
    ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(watches[i].get(),
                                                     handlers[i].get()));
    ASSERT_TRUE(::SetEvent(watches[i]->event_handle()));
  }
  run_loop.Run();
  EXPECT_EQ(completed, kWatchCount);
}

TEST(MessagePumpForIOEventTest, ShardedWatchCanBeStoppedAndRewatched) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  CallbackIOEventHandler unused_handler(BindRepeating([] { ADD_FAILURE(); }));
  auto watches = WatchIOEventsForTesting(MAXIMUM_WAIT_OBJECTS, &unused_handler);
  auto& controller = *watches.back();
  for (int i = 0; i < 1000; ++i) {
    controller.StopWatching();
    ASSERT_TRUE(
        CurrentIOThread::Get()->WatchIOEvent(&controller, &unused_handler));
    ASSERT_TRUE(::SetEvent(controller.event_handle()));
  }
  for (int i = 0; i < 10; ++i) {
    RunLoop run_loop;
    CallbackIOEventHandler handler(run_loop.QuitClosure());
    controller.StopWatching();
    ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&controller, &handler));
    ASSERT_TRUE(::SetEvent(controller.event_handle()));
    run_loop.Run();
  }
}

TEST(MessagePumpForIOEventTest, BusyDirectWatchDoesNotStarveShards) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  RunLoop run_loop;
  MessagePumpForIO::IOEventWatchController busy(CreateIOEventForTesting());
  CallbackIOEventHandler busy_handler(
      BindLambdaForTesting(
          [&] { EXPECT_TRUE(::SetEvent(busy.event_handle())); }),
      /*stop_watching=*/false);
  CallbackIOEventHandler shard_handler(run_loop.QuitClosure());
  CallbackIOEventHandler unused_handler(BindRepeating([] { ADD_FAILURE(); }));
  ScopedClosureRunner stop_busy(
      BindLambdaForTesting([&] { busy.StopWatching(); }));
  ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&busy, &busy_handler));
  auto watches =
      WatchIOEventsForTesting(MAXIMUM_WAIT_OBJECTS - 1, &unused_handler);
  MessagePumpForIO::IOEventWatchController shard_watch(
      CreateIOEventForTesting());
  ASSERT_TRUE(
      CurrentIOThread::Get()->WatchIOEvent(&shard_watch, &shard_handler));
  ASSERT_TRUE(::SetEvent(busy.event_handle()));
  ASSERT_TRUE(::SetEvent(shard_watch.event_handle()));
  run_loop.Run();
}

TEST(MessagePumpForIOEventTest, StoppedShardedWatchCanBeResetAndRewatched) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  CallbackIOEventHandler unexpected_handler(
      BindRepeating([] { ADD_FAILURE() << "A stopped signal was restored"; }),
      /*stop_watching=*/false);
  auto watches =
      WatchIOEventsForTesting(MAXIMUM_WAIT_OBJECTS, &unexpected_handler);
  auto& controller = *watches.back();
  MessagePumpForIO::IOEventWatchController barrier(CreateIOEventForTesting());
  for (int i = 0; i < 1000; ++i) {
    ASSERT_TRUE(::SetEvent(controller.event_handle()));
    controller.StopWatching();
    ASSERT_TRUE(::ResetEvent(controller.event_handle()));
    ASSERT_TRUE(
        CurrentIOThread::Get()->WatchIOEvent(&controller, &unexpected_handler));

    RunLoop run_loop;
    CallbackIOEventHandler barrier_handler(run_loop.QuitClosure());
    ScopedClosureRunner stop_barrier(
        BindLambdaForTesting([&] { barrier.StopWatching(); }));
    ASSERT_TRUE(
        CurrentIOThread::Get()->WatchIOEvent(&barrier, &barrier_handler));
    ASSERT_TRUE(::SetEvent(barrier.event_handle()));
    run_loop.Run();
  }
}

TEST(MessagePumpForIOEventTest, RemovesQueuedShardedCompletions) {
  auto owned_pump = std::make_unique<MessagePumpForIOEventImpl>();
  MessagePumpForIOEventImpl* pump = owned_pump.get();
  SingleThreadTaskExecutor executor(std::move(owned_pump));
  RunLoop run_loop;
  std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
      watches;
  size_t callbacks = 0;
  size_t retained_callbacks = 0;
  CallbackIOEventHandler handler(BindLambdaForTesting([&] {
    EXPECT_EQ(++callbacks, 1u);
    // The other two completions are still queued; stopping must purge them.
    EXPECT_EQ(pump->GetPendingShardCompletionCountForTesting(), 2u);
    for (size_t i = 0; i + 1 < watches.size(); ++i) {
      watches[i]->StopWatching();
    }
    EXPECT_EQ(pump->GetPendingShardCompletionCountForTesting(), 0u);
    EXPECT_TRUE(::SetEvent(watches.back()->event_handle()));
  }));
  CallbackIOEventHandler retained_handler(BindLambdaForTesting([&] {
    ++retained_callbacks;
    run_loop.Quit();
  }));
  ScopedClosureRunner stop_watches(
      BindLambdaForTesting([&] { watches.clear(); }));
  watches =
      WatchIOEventsForTesting(3 * MAXIMUM_WAIT_OBJECTS - 1, &handler, pump);
  watches.push_back(std::make_unique<MessagePumpForIO::IOEventWatchController>(
      CreateIOEventForTesting()));
  ASSERT_TRUE(pump->WatchIOEvent(watches.back().get(), &retained_handler));
  // Signal three watches on the first shard. The pump isn't running, so the
  // shard worker queues all three completions. When the
  // first completion is dispatched, its callback stops all other watches
  // (purging the two still-queued completions) and signals the retained watch.
  constexpr size_t kQueuedCompletions = 3;
  for (size_t i = 0; i < kQueuedCompletions; ++i) {
    ASSERT_TRUE(
        ::SetEvent(watches[MessagePumpForIOEventImpl::kMaxDirectWatches + i]
                       ->event_handle()));
  }
  ASSERT_TRUE(WaitForQueuedShardCompletions(*pump, kQueuedCompletions));
  run_loop.Run();
  for (const auto& controller : watches) {
    ASSERT_TRUE(::SetEvent(controller->event_handle()));
  }

  RunLoop poll_loop;
  CallbackIOEventHandler poll_handler(poll_loop.QuitClosure());
  MessagePumpForIO::IOEventWatchController poll_controller(
      CreateIOEventForTesting());
  ASSERT_TRUE(pump->WatchIOEvent(&poll_controller, &poll_handler));
  ASSERT_TRUE(::SetEvent(poll_controller.event_handle()));
  poll_loop.Run();

  EXPECT_EQ(callbacks, 1u);
  EXPECT_EQ(retained_callbacks, 1u);
}

TEST(MessagePumpForIOEventTest, PumpTeardownDetachesShardedControllers) {
  CallbackIOEventHandler handler(BindRepeating([] { ADD_FAILURE(); }));
  std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
      watches;
  watches.reserve(3 * MAXIMUM_WAIT_OBJECTS);
  {
    MessagePumpForIOEventImpl pump;
    for (size_t i = 0; i < 3 * MAXIMUM_WAIT_OBJECTS; ++i) {
      watches.push_back(
          std::make_unique<MessagePumpForIO::IOEventWatchController>(
              CreateIOEventForTesting()));
      ASSERT_TRUE(pump.WatchIOEvent(watches.back().get(), &handler));
      ASSERT_TRUE(::SetEvent(watches.back()->event_handle()));
    }
  }
  for (const auto& controller : watches) {
    EXPECT_TRUE(::SetEvent(controller->event_handle()));
    controller->StopWatching();
  }
}

TEST(MessagePumpForIOEventTest, ReusesShardsAfterWatchDestruction) {
  constexpr size_t kMaxDirectWatches =
      MessagePumpForIOEventImpl::kMaxDirectWatches;
  constexpr size_t kMaxShardWatches =
      MessagePumpForIOEventImpl::kMaxShardWatches;

  auto owned_pump = std::make_unique<MessagePumpForIOEventImpl>();
  MessagePumpForIOEventImpl* pump = owned_pump.get();
  SingleThreadTaskExecutor executor(std::move(owned_pump));
  CallbackIOEventHandler unexpected_handler(
      BindRepeating([] { ADD_FAILURE(); }));

  auto direct_watches =
      WatchIOEventsForTesting(kMaxDirectWatches, &unexpected_handler, pump);
  EXPECT_EQ(pump->GetShardCountForTesting(), 0u);

  auto first_shard_watches =
      WatchIOEventsForTesting(kMaxShardWatches, &unexpected_handler, pump);
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);
  auto second_shard_watches =
      WatchIOEventsForTesting(1, &unexpected_handler, pump);
  EXPECT_EQ(pump->GetShardCountForTesting(), 2u);

  // Stopping the second shard's watch while the first shard is above half
  // capacity retains the empty second shard within the hysteresis window.
  second_shard_watches.clear();
  EXPECT_EQ(pump->GetShardCountForTesting(), 2u);

  // Dropping the first shard to half capacity prunes the empty second shard.
  first_shard_watches.resize(kMaxShardWatches / 2);
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);

  // Stopping the remaining sharded watches while keeping direct watches above
  // half capacity retains the empty first shard for reuse.
  first_shard_watches.clear();
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);

  RunLoop reused_shard_loop;
  size_t callbacks = 0;
  CallbackIOEventHandler handler(BindLambdaForTesting([&] {
    ++callbacks;
    reused_shard_loop.Quit();
  }));
  MessagePumpForIO::IOEventWatchController extra(CreateIOEventForTesting());
  ASSERT_TRUE(pump->WatchIOEvent(&extra, &handler));
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);
  ASSERT_TRUE(::SetEvent(extra.event_handle()));
  reused_shard_loop.Run();
  EXPECT_EQ(callbacks, 1u);
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);

  // Dropping direct watches to half capacity while the first shard is empty
  // prunes it, and re-filling past the direct limit spawns a new shard.
  direct_watches.resize(kMaxDirectWatches / 2 + 1);
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);
  direct_watches.pop_back();
  EXPECT_EQ(pump->GetShardCountForTesting(), 0u);

  direct_watches.clear();
  direct_watches =
      WatchIOEventsForTesting(kMaxDirectWatches, &unexpected_handler, pump);
  EXPECT_EQ(pump->GetShardCountForTesting(), 0u);

  RunLoop respawned_shard_loop;
  CallbackIOEventHandler respawned_handler(BindLambdaForTesting([&] {
    ++callbacks;
    respawned_shard_loop.Quit();
  }));
  ASSERT_TRUE(pump->WatchIOEvent(&extra, &respawned_handler));
  EXPECT_EQ(pump->GetShardCountForTesting(), 1u);
  ASSERT_TRUE(::SetEvent(extra.event_handle()));
  respawned_shard_loop.Run();
  EXPECT_EQ(callbacks, 2u);
}

// Threads such as BrowserProcessIOThread disallow blocking and //base sync
// primitives. Sharded watches must still work there even though
// StopWatching() waits for the worker to drop the handle, and pruning a shard
// or tearing down the pump joins worker threads.
TEST(MessagePumpForIOEventTest, ShardsWorkWhenSyncPrimitivesDisallowed) {
  constexpr size_t kMaxDirectWatches =
      MessagePumpForIOEventImpl::kMaxDirectWatches;
  CallbackIOEventHandler unexpected_handler(
      BindRepeating([] { ADD_FAILURE(); }));
  // Outlives `pump` so that pump teardown joins a non-empty shard.
  std::vector<std::unique_ptr<MessagePumpForIO::IOEventWatchController>>
      retained_watches;

  ScopedDisallowBlocking disallow_blocking;
  ScopedDisallowBaseSyncPrimitives disallow_sync_primitives;
  MessagePumpForIOEventImpl pump;
  auto direct_watches =
      WatchIOEventsForTesting(kMaxDirectWatches, &unexpected_handler, &pump);
  auto sharded_watches = WatchIOEventsForTesting(1, &unexpected_handler, &pump);
  ASSERT_EQ(pump.GetShardCountForTesting(), 1u);

  // Stopping the only sharded watch with the direct tier at half capacity
  // waits for the worker to apply the removal, then joins the empty shard.
  direct_watches.resize(kMaxDirectWatches / 2);
  sharded_watches.clear();
  EXPECT_EQ(pump.GetShardCountForTesting(), 0u);

  auto refill_watches = WatchIOEventsForTesting(
      kMaxDirectWatches - direct_watches.size(), &unexpected_handler, &pump);
  retained_watches = WatchIOEventsForTesting(1, &unexpected_handler, &pump);
  EXPECT_EQ(pump.GetShardCountForTesting(), 1u);
}

// Verifies that sharded completions already queued on the pump get dispatched
// before DoIdleWork(). That includes a completion queued from inside a sharded
// callback.
//
// Both signals call WaitForQueuedShardCompletions() before the pump polls, so
// `completion_available_event_` is already signaled when WaitForIOCompletion(0)
// runs. The completion is therefore always dispatched via TakeCompletion(), and
// PollForIOBeforeIdle() is never reached. This test does not cover the
// ShardState::PollForCompletion() / WaitShard::PollForCompletion() hit path or
// the `next_poll_shard_index_` cursor. Covering those would mean racing the
// shard worker thread, and this test avoids that race so it stays
// deterministic.
TEST(MessagePumpForIOEventTest, DispatchesQueuedShardedCompletionBeforeIdle) {
  MessagePumpForIOEventImpl pump;
  testing::NiceMock<MockMessagePumpDelegate> delegate(MessagePumpType::IO);
  CallbackIOEventHandler unexpected_handler(
      BindRepeating([] { ADD_FAILURE(); }));
  auto direct_watches = WatchIOEventsForTesting(
      MessagePumpForIOEventImpl::kMaxDirectWatches, &unexpected_handler, &pump);

  size_t callbacks = 0;
  MessagePumpForIO::IOEventWatchController sharded_watch(
      CreateIOEventForTesting());
  CallbackIOEventHandler sharded_handler(
      BindLambdaForTesting([&] {
        if (++callbacks == 1u) {
          // A completion queued from inside the callback must also be
          // dispatched by the zero-timeout poll before DoIdleWork().
          EXPECT_TRUE(::SetEvent(sharded_watch.event_handle()));
          EXPECT_TRUE(WaitForQueuedShardCompletions(pump));
        } else {
          sharded_watch.StopWatching();
          pump.Quit();
        }
      }),
      /*stop_watching=*/false);
  ASSERT_TRUE(::SetEvent(sharded_watch.event_handle()));
  ASSERT_TRUE(pump.WatchIOEvent(&sharded_watch, &sharded_handler));
  ASSERT_TRUE(WaitForQueuedShardCompletions(pump));

  EXPECT_CALL(delegate, DoWork).WillRepeatedly([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(delegate, DoIdleWork).Times(0);
  pump.Run(&delegate);
  EXPECT_EQ(callbacks, 2u);
}

TEST(MessagePumpForIOEventTest, RejectsLegacyIOCPRegistration) {
  MessagePumpForIOEventImpl pump;
  EXPECT_NOTREACHED_DEATH(pump.RegisterIOHandler(nullptr, nullptr));
  EXPECT_NOTREACHED_DEATH(pump.RegisterJobObject(nullptr, nullptr));
}

TEST(MessagePumpForIOEventTest, CompletionPortRemainsDefault) {
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  EXPECT_FALSE(CurrentIOThread::Get()->SupportsIOEvents());
}

TEST(MessagePumpForIOEventTest, WakesForCrossThreadTasks) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  RunLoop run_loop;
  Thread worker("Event pump wakeup");
  ASSERT_TRUE(worker.Start());
  ASSERT_TRUE(worker.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<SingleThreadTaskRunner> origin, OnceClosure quit) {
            EXPECT_TRUE(origin->PostTask(FROM_HERE, std::move(quit)));
          },
          executor.task_runner(), run_loop.QuitClosure())));
  run_loop.Run();
}

TEST(MessagePumpForIOEventTest, RepeatedWakeupsAreConsumedOnlyWhenWaiting) {
  MessagePumpForIOEventImpl pump;
  testing::NiceMock<MockMessagePumpDelegate> delegate(MessagePumpType::IO);
  testing::InSequence sequence;
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(delegate, DoIdleWork);
  EXPECT_CALL(delegate, DoWork).WillOnce([&] {
    pump.ScheduleWork();
    pump.ScheduleWork();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(delegate, DoIdleWork);
  EXPECT_CALL(delegate, DoWork).WillOnce([&] {
    pump.Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });

  pump.ScheduleWork();
  pump.ScheduleWork();
  pump.Run(&delegate);
}

TEST(MessagePumpForIOEventTest, WorkScheduledImmediatelyBeforeWaitWakesPump) {
  MessagePumpForIOEventImpl pump;
  class Delegate : public testing::NiceMock<MockMessagePumpDelegate> {
   public:
    explicit Delegate(MessagePumpForIOEventImpl& pump)
        : testing::NiceMock<MockMessagePumpDelegate>(MessagePumpType::IO),
          pump_(&pump) {}

    void BeforeWait() override { pump_->ScheduleWork(); }

   private:
    const raw_ptr<MessagePumpForIOEventImpl> pump_;
  } delegate(pump);
  testing::InSequence sequence;
  EXPECT_CALL(delegate, DoWork).WillOnce([] {
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  EXPECT_CALL(delegate, DoIdleWork);
  EXPECT_CALL(delegate, DoWork).WillOnce([&] {
    pump.Quit();
    return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
  });
  pump.Run(&delegate);
}

TEST(MessagePumpForIOEventTest, RotationAndCallbackRemovalPreserveWatchOrder) {
  test::ScopedMessagePumpForIOEventFactoryForTesting factory;
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  RunLoop run_loop;
  std::vector<int> order;
  MessagePumpForIO::IOEventWatchController first(CreateIOEventForTesting());
  MessagePumpForIO::IOEventWatchController middle(CreateIOEventForTesting());
  MessagePumpForIO::IOEventWatchController last(CreateIOEventForTesting());
  CallbackIOEventHandler first_handler(
      BindLambdaForTesting([&] {
        order.push_back(1);
        middle.StopWatching();
        EXPECT_TRUE(::SetEvent(first.event_handle()));
        if (order.size() == 3) {
          run_loop.Quit();
        }
      }),
      /*stop_watching=*/false);
  CallbackIOEventHandler middle_handler(BindLambdaForTesting([&] {
    ADD_FAILURE() << "An unwatched event was dispatched";
    run_loop.Quit();
  }));
  CallbackIOEventHandler last_handler(
      BindLambdaForTesting([&] { order.push_back(3); }));
  ScopedClosureRunner stop_watches(BindLambdaForTesting([&] {
    first.StopWatching();
    middle.StopWatching();
    last.StopWatching();
  }));
  ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&first, &first_handler));
  ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&middle, &middle_handler));
  ASSERT_TRUE(CurrentIOThread::Get()->WatchIOEvent(&last, &last_handler));
  ASSERT_TRUE(::SetEvent(first.event_handle()));
  ASSERT_TRUE(::SetEvent(middle.event_handle()));
  ASSERT_TRUE(::SetEvent(last.event_handle()));
  run_loop.Run();
  EXPECT_THAT(order, testing::ElementsAre(1, 3, 1));
}
#endif

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

TEST_P(MessagePumpTest, PeriodicYieldToLooper) {
  testing::InSequence sequence;
  MessagePumpType pump_type = GetParam();
  testing::StrictMock<MockMessagePumpDelegate> delegate(pump_type);

  SimpleTestTickClock test_clock;
  test_clock.Advance(Seconds(1));
  auto& yield_checker = android::YieldToLooperChecker::GetInstance();
  yield_checker.SetTickClockForTesting(&test_clock);
  ScopedClosureRunner reset_checker(BindLambdaForTesting([&yield_checker] {
    yield_checker.SetTickClockForTesting(nullptr);
    yield_checker.InitializeFeatures();
  }));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      android::kYieldToLooperPeriodically,
      {{android::kYieldToLooperPeriod.name, "20ms"}});
  yield_checker.InitializeFeatures();

  uint32_t initial_work_enters = GetAndroidNonDelayedWorkEnterCount();
  constexpr MessagePump::Delegate::NextWorkInfo kImmediateWork;
  ASSERT_TRUE(kImmediateWork.is_immediate());

  // First DoWork() advances by 10ms (under the 20ms period) and has immediate
  // follow-up work, so DoNonDelayedLooperWork() should not yield yet.
  EXPECT_CALL(delegate, DoWork).WillOnce([&test_clock, kImmediateWork] {
    test_clock.Advance(Milliseconds(10));
    return kImmediateWork;
  });

  // Second DoWork() runs in the same DoNonDelayedLooperWork() batch and
  // advances by another 10ms (reaching `kYieldToLooperPeriod`), triggering a
  // yield to the Looper on UI pumps.
  EXPECT_CALL(delegate, DoWork)
      .WillOnce([&test_clock, pump_type, initial_work_enters, kImmediateWork] {
        uint32_t expected_enters = (pump_type == MessagePumpType::UI) ? 1 : 0;
        EXPECT_EQ(initial_work_enters + expected_enters,
                  GetAndroidNonDelayedWorkEnterCount());
        test_clock.Advance(Milliseconds(10));
        return kImmediateWork;
      });

  // Third DoWork() runs after resetting `batch_start_`. Returning immediate
  // work without advancing past 20ms does not trigger another yield.
  EXPECT_CALL(delegate, DoWork)
      .WillOnce([pump_type, initial_work_enters, kImmediateWork] {
        uint32_t expected_enters = (pump_type == MessagePumpType::UI) ? 2 : 0;
        EXPECT_EQ(initial_work_enters + expected_enters,
                  GetAndroidNonDelayedWorkEnterCount());
        return kImmediateWork;
      });

  // Fourth DoWork() still runs in the 2nd batch on UI pumps and quits the loop.
  EXPECT_CALL(delegate, DoWork).WillOnce([this] {
    message_pump_->Quit();
    return MessagePump::Delegate::NextWorkInfo{.delayed_run_time =
                                                   TimeTicks::Max()};
  });

  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->Run(&delegate);

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

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_APPLE)
class MessagePumpWakeupCounterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MessagePumpWakeupCounter::GetForCurrentThread().ResetForTesting();
  }
};

TEST_F(MessagePumpWakeupCounterTest, IoPumpRecordsWakeup) {
  SingleThreadTaskExecutor executor(MessagePumpType::IO);
  MessagePumpWakeupCounter::InitializeForCurrentThread("TestIO");
  HistogramTester histogram_tester;

  base::RunLoop run_loop;
  base::Thread background_thread("BackgroundThread");
  background_thread.Start();

  background_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<SingleThreadTaskRunner> io_runner,
             base::OnceClosure quit_closure) {
            base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
            io_runner->PostTask(FROM_HERE, std::move(quit_closure));
          },
          executor.task_runner(), run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "Scheduling.MessagePump.WakeupCount2.TestIO", 1, 1);
}

TEST_F(MessagePumpWakeupCounterTest, BusyLoopSkipsRecordingSample) {
  MessagePumpWakeupCounter::InitializeForCurrentThread("BusyLoopThread");
  MessagePumpDefault pump;
  pump.SetBusyLoop(Milliseconds(10));
  pump.SetShouldBusyLoopForTesting(true);
  ASSERT_TRUE(pump.ShouldBusyLoop());
  testing::NiceMock<MockMessagePumpDelegate> delegate(MessagePumpType::DEFAULT);
  EXPECT_CALL(delegate, DoWork)
      .WillOnce([&] {
        pump.ScheduleWork();
        return MessagePump::Delegate::NextWorkInfo{
            .delayed_run_time =
                TimeTicks::Now() + TestTimeouts::tiny_timeout()};
      })
      .WillOnce([&] {
        pump.Quit();
        return MessagePump::Delegate::NextWorkInfo{TimeTicks::Max()};
      });
  HistogramTester histogram_tester;

  pump.Run(&delegate);

  histogram_tester.ExpectBucketCount(
      "Scheduling.MessagePump.WakeupCount2.BusyLoopThread", 1, 0);
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
// Regression test: if a native nested loop goes idle with a delayed task
// pending, HandleWorkMessage() installs MessagePumpForUI's native timer. If the
// RunLoop is later quit from that native loop, the timer must not leak past it.
TEST(MessagePumpTest, NativeTimerDoesNotLeakPastRunLoopQuit) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);
  constexpr TimeDelta kPumpTimerDelay = Milliseconds(20);

  // Makes the native nested loop install the pump's native timer when idle.
  executor.task_runner()->PostDelayedTask(FROM_HERE, DoNothing(),
                                          kPumpTimerDelay);

  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
        // Runs in the native loop's first pass. WM_APP is then retrieved
        // after that pass has gone idle and installed the timer.
        SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, BindOnce([] { ::PostMessage(nullptr, WM_APP, 0, 0); }));
        MSG msg;
        while (::GetMessage(&msg, nullptr, 0, 0)) {
          if (msg.message == WM_APP) {
            run_loop.Quit();
            break;
          }
          ::DispatchMessage(&msg);
        }
      }));
  run_loop.Run();

  // A leaked native timer would deliver a WM_TIMER.
  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::MsgWaitForMultipleObjectsEx(
                0, nullptr, TestTimeouts::tiny_timeout().InMilliseconds(),
                QS_TIMER, MWMO_INPUTAVAILABLE));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
