// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker_impl.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time_override.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

using base::test::TestFuture;
using content::MockSmartCardContextFactory;
using device::mojom::SmartCardContext;
using device::mojom::SmartCardError;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateIn;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardStatusChangeResult;
using device::mojom::SmartCardSuccess;

using testing::_;
using testing::ElementsAre;
using testing::InSequence;
using testing::StrictMock;
using testing::UnorderedElementsAre;

// Used by Google Test to print human-readable error messages involving
// ReaderInfo comparisons.
void PrintTo(const SmartCardReaderTracker::ReaderInfo& reader,
             std::ostream* os) {
  *os << "ReaderInfo(" << reader.name;

  if (reader.unavailable) {
    *os << ", unavailable";
  }

  if (reader.empty) {
    *os << ", empty";
  }

  if (reader.present) {
    *os << ", present";
  }

  if (reader.exclusive) {
    *os << ", exclusive";
  }

  if (reader.inuse) {
    *os << ", inuse";
  }

  if (reader.mute) {
    *os << ", mute";
  }

  if (reader.unpowered) {
    *os << ", unpowered";
  }

  *os << ", event_count=" << reader.event_count;

  *os << ", ATR{";
  bool first = true;
  for (uint8_t num : reader.answer_to_reset) {
    if (!first) {
      *os << ",";
    }
    // Treat it as a number instead of as a char.
    *os << unsigned(num);
    first = false;
  }
  *os << "})";
}

namespace {

void ExpectUnaware(const SmartCardReaderStateIn& state_in) {
  // This is the only field expected to be set/true.
  EXPECT_TRUE(state_in.current_state->unaware);

  // All the rest should be false or 0.
  EXPECT_FALSE(state_in.current_state->ignore);
  EXPECT_FALSE(state_in.current_state->changed);
  EXPECT_FALSE(state_in.current_state->unknown);
  EXPECT_FALSE(state_in.current_state->unavailable);
  EXPECT_FALSE(state_in.current_state->empty);
  EXPECT_FALSE(state_in.current_state->present);
  EXPECT_FALSE(state_in.current_state->exclusive);
  EXPECT_FALSE(state_in.current_state->inuse);
  EXPECT_FALSE(state_in.current_state->mute);
  EXPECT_FALSE(state_in.current_state->unpowered);
  EXPECT_EQ(state_in.current_count, 0);
}

void ExpectEmpty(const SmartCardReaderStateFlags& state_flags) {
  EXPECT_FALSE(state_flags.unaware);
  EXPECT_FALSE(state_flags.ignore);
  EXPECT_FALSE(state_flags.changed);
  EXPECT_FALSE(state_flags.unknown);
  EXPECT_FALSE(state_flags.unavailable);

  // The only flag expected to be true.
  EXPECT_TRUE(state_flags.empty);

  EXPECT_FALSE(state_flags.present);
  EXPECT_FALSE(state_flags.exclusive);
  EXPECT_FALSE(state_flags.inuse);
  EXPECT_FALSE(state_flags.mute);
  EXPECT_FALSE(state_flags.unpowered);
}

void ExpectInUse(const SmartCardReaderStateFlags& state_flags) {
  EXPECT_FALSE(state_flags.unaware);
  EXPECT_FALSE(state_flags.ignore);
  EXPECT_FALSE(state_flags.changed);
  EXPECT_FALSE(state_flags.unknown);
  EXPECT_FALSE(state_flags.unavailable);
  EXPECT_FALSE(state_flags.empty);

  // There's a card present in the reader.
  EXPECT_TRUE(state_flags.present);

  EXPECT_FALSE(state_flags.exclusive);

  // And that card is in use.
  EXPECT_TRUE(state_flags.inuse);

  EXPECT_FALSE(state_flags.mute);
  EXPECT_FALSE(state_flags.unpowered);
}

enum class StateFlags {
  kEmpty,
  kNowEmpty,
  kInUse,
  kNowUnknown,
};

enum class ReaderState {
  kEmpty,
  kInUse,
};

SmartCardReaderStateOutPtr CreateStateOut(std::string name,
                                          StateFlags state,
                                          uint16_t event_count = 0,
                                          std::vector<uint8_t> atr = {}) {
  auto state_flags = SmartCardReaderStateFlags::New();
  switch (state) {
    case StateFlags::kEmpty:
      state_flags->empty = true;
      break;
    case StateFlags::kNowEmpty:
      state_flags->changed = true;
      state_flags->empty = true;
      break;
    case StateFlags::kInUse:
      state_flags->present = true;
      state_flags->inuse = true;
      break;
    case StateFlags::kNowUnknown:
      state_flags->changed = true;
      state_flags->unknown = true;
      break;
  }

  return SmartCardReaderStateOut::New(std::move(name), std::move(state_flags),
                                      event_count, std::move(atr));
}

SmartCardReaderTracker::ReaderInfo CreateReaderInfo(
    std::string name,
    ReaderState state,
    uint16_t event_count = 0,
    std::vector<uint8_t> atr = {}) {
  SmartCardReaderTracker::ReaderInfo info;
  info.name = std::move(name);
  info.event_count = event_count;
  info.answer_to_reset = std::move(atr);

  switch (state) {
    case ReaderState::kEmpty:
      info.empty = true;
      break;
    case ReaderState::kInUse:
      info.present = true;
      info.inuse = true;
      break;
  }

  return info;
}

void ReportStateOut(SmartCardContext::GetStatusChangeCallback callback,
                    SmartCardReaderStateOutPtr s) {
  std::vector<SmartCardReaderStateOutPtr> states_out;
  states_out.push_back(std::move(s));

  auto result =
      SmartCardStatusChangeResult::NewReaderStates(std::move(states_out));

  std::move(callback).Run(std::move(result));
}

void ReportStateOut(SmartCardContext::GetStatusChangeCallback callback,
                    SmartCardReaderStateOutPtr s1,
                    SmartCardReaderStateOutPtr s2) {
  std::vector<SmartCardReaderStateOutPtr> states_out;
  states_out.push_back(std::move(s1));
  states_out.push_back(std::move(s2));

  auto result =
      SmartCardStatusChangeResult::NewReaderStates(std::move(states_out));

  std::move(callback).Run(std::move(result));
}

class SmartCardReaderTrackerImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

class MockTrackerObserver : public SmartCardReaderTracker::Observer {
 public:
  MOCK_METHOD(void,
              OnReaderRemoved,
              (const std::string& reader_name),
              (override));

  MOCK_METHOD(void,
              OnReaderChanged,
              (const SmartCardReaderTracker::ReaderInfo& reader_info),
              (override));

  MOCK_METHOD(void, OnError, (SmartCardError error), (override));
};

TEST_F(SmartCardReaderTrackerImplTest, ReaderChanged) {
  MockSmartCardContextFactory mock_context_factory;
  SmartCardReaderTrackerImpl tracker(mock_context_factory.GetRemote());
  StrictMock<MockTrackerObserver> observer;

  {
    InSequence s;

    // Request what readers are currently available.
    mock_context_factory.ExpectListReaders({"Reader A", "Reader B"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              ExpectUnaware(*states_in[1]);

              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", StateFlags::kEmpty),
                  CreateStateOut("Reader B", StateFlags::kInUse,
                                 /*event_count=*/1,
                                 std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            });

    // Request to be notified of state changes on those readers.
    // SmartCardContext reports that "Reader B" has changed (card was removed,
    // thus it's now empty).
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectInUse(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 1);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", StateFlags::kEmpty),
                             // Reader B has changed. It's now empty as well.
                             CreateStateOut("Reader B", StateFlags::kNowEmpty,
                                            /*event_count=*/2));
            });

    EXPECT_CALL(observer, OnReaderChanged(
                              CreateReaderInfo("Reader B", ReaderState::kEmpty,
                                               /*event_count=*/2, {})));

    ////
    // Now rinse and repeat

    // Request what readers are currently available.
    // Still the same readers.
    mock_context_factory.ExpectListReaders({"Reader A", "Reader B"});

    // Since ListReaders did not return any reader unknown to the tracker,
    // it will now skip to waiting to be notified on any changes.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              // Note that the tracker now states that Reader B is empty as
              // well.
              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectEmpty(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 2);

              std::vector<SmartCardReaderStateOutPtr> states_out;

              // Give an error, which will cause the tracker to go back into
              // Uninitialized state, making no further queries. Letting us end
              // this test.
              std::move(callback).Run(SmartCardStatusChangeResult::NewError(
                  SmartCardError::kNoService));
            });

    EXPECT_CALL(observer, OnError(SmartCardError::kNoService));

    // This unrecoverable failure should cause the tracker to drop its smart
    // card context and stop tracking.
    EXPECT_CALL(mock_context_factory, ContextDisconnected());
  }

  TestFuture<std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>>
      start_future;
  tracker.Start(&observer, start_future.GetCallback());

  std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>> readers =
      start_future.Take();
  ASSERT_TRUE(readers.has_value());

  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// If Start() is called while tracking is already taking place and
// kMinRefreshInterval has elapsed since tracking has started, it should cause
// tracking to be restarted. ie, a new list of readers being fetched, etc.
TEST_F(SmartCardReaderTrackerImplTest, Restart) {
  MockSmartCardContextFactory mock_context_factory;
  SmartCardReaderTrackerImpl tracker(mock_context_factory.GetRemote());
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;
  StrictMock<MockTrackerObserver> observer;

  {
    InSequence s;

    // Request what readers are currently available.
    mock_context_factory.ExpectListReaders({"Reader A", "Reader B"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              ExpectUnaware(*states_in[1]);

              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", StateFlags::kEmpty),
                  CreateStateOut("Reader B", StateFlags::kInUse,
                                 /*event_count=*/1,
                                 std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            });

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [&tracking_get_status_callback](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectInUse(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 1);

              // Handle that in the upcoming Cancel() call.
              tracking_get_status_callback.SetValue(std::move(callback));
            });

    EXPECT_CALL(mock_context_factory, Cancel(_))
        .WillOnce([&tracking_get_status_callback](
                      SmartCardContext::CancelCallback callback) {
          // The cancel call succeeded.
          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));

          // And the ongoing GetStatusChange() PC/SC call was correspondingly
          // canceled.
          tracking_get_status_callback.Take().Run(
              SmartCardStatusChangeResult::NewError(
                  SmartCardError::kCancelled));
        });

    ////
    // Now rinse and repeat

    // Request what readers are currently available.
    // Still the same readers.
    mock_context_factory.ExpectListReaders({"Reader A", "Reader B"});

    // Since ListReaders did not return any reader unknown to the tracker,
    // it will now skip to waiting to be notified on any changes.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectInUse(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 1);

              std::vector<SmartCardReaderStateOutPtr> states_out;

              // Give an error, which will cause the tracker to go back into
              // Uninitialized state, making no further queries. Letting us end
              // this test.
              std::move(callback).Run(
                  device::mojom::SmartCardStatusChangeResult::NewError(
                      SmartCardError::kNoService));
            });

    // The error given by the last GetStatusChange() call.
    EXPECT_CALL(observer, OnError(SmartCardError::kNoService));

    // This unrecoverable failure should cause the tracker to drop its smart
    // card context and stop tracking.
    EXPECT_CALL(mock_context_factory, ContextDisconnected());
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() { return base::Time::FromSecondsSinceUnixEpoch(0); }, nullptr,
        nullptr);

    TestFuture<std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>>
        start_future;
    // The first start() call, at t0.
    tracker.Start(&observer, start_future.GetCallback());

    std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>> readers =
        start_future.Take();
    ASSERT_TRUE(readers.has_value());
    EXPECT_THAT(*readers,
                UnorderedElementsAre(
                    CreateReaderInfo("Reader A", ReaderState::kEmpty),
                    CreateReaderInfo("Reader B", ReaderState::kInUse,
                                     /*event_count=*/1,
                                     std::vector<uint8_t>({1u, 2u, 3u, 4u}))));
  }

  // Wait until SmartCardReaderTracerImpl is in the Tracking state, waiting
  // for the GetStatusChange() call result.
  ASSERT_TRUE(tracking_get_status_callback.Wait());

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          return base::Time::FromSecondsSinceUnixEpoch(
              SmartCardReaderTrackerImpl::kMinRefreshInterval.InSecondsF() * 2);
        },
        nullptr, nullptr);

    TestFuture<std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>>
        start_future;
    // The second start() call at t0 + (kMinRefreshInterval*2).
    // Will make the tracker cancel the currently outstanding GetStatusChange()
    // request and restart from the ListReaders() call.
    tracker.Start(&observer, start_future.GetCallback());

    std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>> readers =
        start_future.Take();
    ASSERT_TRUE(readers.has_value());
    EXPECT_THAT(*readers,
                UnorderedElementsAre(
                    CreateReaderInfo("Reader A", ReaderState::kEmpty),
                    CreateReaderInfo("Reader B", ReaderState::kInUse,
                                     /*event_count=*/1,
                                     std::vector<uint8_t>({1u, 2u, 3u, 4u}))));
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Test that tracker will cancel its outstanding GetStatusChange() request and
// stop tracking when the last observer leaves.
TEST_F(SmartCardReaderTrackerImplTest, StopWhenTracking) {
  MockSmartCardContextFactory mock_context_factory;
  SmartCardReaderTrackerImpl tracker(mock_context_factory.GetRemote());
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;
  StrictMock<MockTrackerObserver> observer;

  {
    InSequence s;

    // Request what readers are currently available.
    mock_context_factory.ExpectListReaders({"Reader A"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", StateFlags::kEmpty));
            });

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [&tracking_get_status_callback](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              // Handle that in the upcoming Cancel() call.
              tracking_get_status_callback.SetValue(std::move(callback));
            });

    // When tracker.Stop() is called, the tracker should cancel the outstanding
    // GetStatusChange() request.
    EXPECT_CALL(mock_context_factory, Cancel(_))
        .WillOnce([&tracking_get_status_callback](
                      SmartCardContext::CancelCallback callback) {
          // The cancel call succeeded.
          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));

          // And the ongoing GetStatusChange() PC/SC call was correspondingly
          // canceled.
          tracking_get_status_callback.Take().Run(
              SmartCardStatusChangeResult::NewError(
                  SmartCardError::kCancelled));
        });

    // Tracker should then drop its smart card context as it will no longer
    // track readers.
    EXPECT_CALL(mock_context_factory, ContextDisconnected());
  }

  // Start()
  {
    TestFuture<std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>>
        start_future;
    tracker.Start(&observer, start_future.GetCallback());
    ASSERT_TRUE(start_future.Wait());
  }

  // Wait until the tracker is in Tracking state, with an outstanding
  // GetStatusChange request.
  ASSERT_TRUE(tracking_get_status_callback.Wait());

  // Then Stop()
  tracker.Stop(&observer);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(SmartCardReaderTrackerImplTest, ReaderRemoved) {
  MockSmartCardContextFactory mock_context_factory;
  SmartCardReaderTrackerImpl tracker(mock_context_factory.GetRemote());
  StrictMock<MockTrackerObserver> observer;

  {
    InSequence s;

    // Request what readers are currently available.
    mock_context_factory.ExpectListReaders({"Reader A"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", StateFlags::kEmpty));
            });

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              // Reader A has been removed. It's now unknown.
              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", StateFlags::kNowUnknown));
            });

    // Tracker will notify observers about this removal.
    EXPECT_CALL(observer, OnReaderRemoved("Reader A"));

    // As there are no readers left, tracker should stop on its own and drop its
    // smart card context.
    EXPECT_CALL(mock_context_factory, ContextDisconnected());
  }

  // Start()
  {
    TestFuture<std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>>
        start_future;
    tracker.Start(&observer, start_future.GetCallback());
    ASSERT_TRUE(start_future.Wait());
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace
