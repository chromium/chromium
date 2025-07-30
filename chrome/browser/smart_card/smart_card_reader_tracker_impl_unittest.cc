// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

using base::test::InvokeFuture;
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

using testing::ElementsAre;
using testing::InSequence;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::WithArg;
using testing::WithArgs;

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

using OptionalReaderList =
    std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>;

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

enum class ReaderState { kEmpty, kInUse, kUnknown };

enum class ReaderInfoState {
  kEmpty,
  kInUse,
};

SmartCardReaderStateOutPtr CreateStateOut(std::string name,
                                          ReaderState state,
                                          bool changed = false,
                                          uint16_t event_count = 0,
                                          std::vector<uint8_t> atr = {}) {
  auto state_flags = SmartCardReaderStateFlags::New();
  state_flags->changed = changed;

  switch (state) {
    case ReaderState::kEmpty:
      state_flags->empty = true;
      break;
    case ReaderState::kInUse:
      state_flags->present = true;
      state_flags->inuse = true;
      break;
    case ReaderState::kUnknown:
      state_flags->unknown = true;
      break;
  }

  return SmartCardReaderStateOut::New(std::move(name), std::move(state_flags),
                                      event_count, std::move(atr));
}

SmartCardReaderTracker::ReaderInfo CreateReaderInfo(
    std::string name,
    ReaderInfoState state,
    uint16_t event_count = 0,
    std::vector<uint8_t> atr = {}) {
  SmartCardReaderTracker::ReaderInfo info;
  info.name = std::move(name);
  info.event_count = event_count;
  info.answer_to_reset = std::move(atr);

  switch (state) {
    case ReaderInfoState::kEmpty:
      info.empty = true;
      break;
    case ReaderInfoState::kInUse:
      info.present = true;
      info.inuse = true;
      break;
  }

  return info;
}

template <typename... T>
void ReportStateOut(SmartCardContext::GetStatusChangeCallback callback,
                    T... states) {
  std::vector<SmartCardReaderStateOutPtr> states_out;
  (states_out.push_back(std::move(states)), ...);

  auto result =
      SmartCardStatusChangeResult::NewReaderStates(std::move(states_out));

  std::move(callback).Run(std::move(result));
}

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

class SmartCardReaderTrackerImplTest : public testing::Test {
 protected:
  SmartCardReaderTrackerImplTest()
      : tracker_(mock_context_factory_.GetRemote()) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockSmartCardContextFactory> mock_context_factory_;
  SmartCardReaderTrackerImpl tracker_;
  StrictMock<MockTrackerObserver> observer_;
  TestFuture<void> context_disconnected_;
};

TEST_F(SmartCardReaderTrackerImplTest, CreateContextError) {
  {
    InSequence s;

    mock_context_factory_.ExpectCreateContextError(SmartCardError::kNoService);

    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  // This is treated as an error, no value should appear.
  ASSERT_FALSE(readers.has_value());
  // Here RunUntilIdle is essential - to wait for WaitContext destruction
  // scheduled by ChangeState through SequencedTaskRunner::DeleteSoon. Without
  // it, a memory leak will occur on test end.
  task_environment_.RunUntilIdle();
}

TEST_F(SmartCardReaderTrackerImplTest, ListReadersError) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    mock_context_factory_.ExpectListReadersError(
        SmartCardError::kInternalError);

    EXPECT_CALL(observer_, OnError(SmartCardError::kInternalError));

    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  // This is treated as an error, no value should appear.
  ASSERT_FALSE(readers.has_value());

  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest,
       GetStatusChangeErrorInWaitInitialReaderStatus) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    mock_context_factory_.ExpectListReaders({"Reader A"});

    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(
            WithArg<2>([](SmartCardContext::GetStatusChangeCallback callback) {
              std::move(callback).Run(SmartCardStatusChangeResult::NewError(
                  SmartCardError::kNoService));
            }));

    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));

    EXPECT_CALL(mock_context_factory_, ContextDisconnected)
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  // This is treated as an error, no value should appear.
  ASSERT_FALSE(readers.has_value());

  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest, NoReaders) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    mock_context_factory_.ExpectListReadersError(
        SmartCardError::kNoReadersAvailable);

    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  // This is not treated as an error, should yield empty list.
  ASSERT_TRUE(readers.has_value());
  EXPECT_TRUE(readers->empty());

  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest, ReaderChanged) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Request what readers are currently available.
    mock_context_factory_.ExpectListReaders({"Reader A", "Reader B"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              ExpectUnaware(*states_in[1]);

              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", ReaderState::kEmpty),
                  CreateStateOut("Reader B", ReaderState::kInUse,
                                 /*changed=*/false,
                                 /*event_count=*/1,
                                 std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            }));

    // Request to be notified of state changes on those readers.
    // SmartCardContext reports that "Reader B" has changed (card was removed,
    // thus it's now empty).
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectInUse(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 1);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kEmpty),
                             // Reader B has changed. It's now empty as well.
                             CreateStateOut("Reader B", ReaderState::kEmpty,
                                            /*changed=*/true,
                                            /*event_count=*/2));
            }));

    EXPECT_CALL(observer_, OnReaderChanged(CreateReaderInfo(
                               "Reader B", ReaderInfoState::kEmpty,
                               /*event_count=*/2, {})));

    ////
    // Now rinse and repeat

    // Request what readers are currently available.
    // Still the same readers.
    mock_context_factory_.ExpectListReaders({"Reader A", "Reader B"});

    // Since ListReaders did not return any reader unknown to the tracker,
    // it will now skip to waiting to be notified on any changes.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
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
            }));

    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));

    // This unrecoverable failure should cause the tracker to drop its smart
    // card context and stop tracking.
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());

  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderInfoState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderInfoState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  ASSERT_TRUE(context_disconnected_.Wait());
}

// Test that if Start() is called while tracking is already taking place,
// and kMinRefreshInterval has NOT elapsed since tracking has started,
// the tracker just fulfills the request from the cache without
// restarting the tracking.
TEST_F(SmartCardReaderTrackerImplTest, DontRestart) {
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;

  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Request what readers are currently available.
    mock_context_factory_.ExpectListReaders({"Reader A", "Reader B"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              ExpectUnaware(*states_in[1]);

              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", ReaderState::kEmpty),
                  CreateStateOut("Reader B", ReaderState::kInUse,
                                 /*changed=*/false,
                                 /*event_count=*/1,
                                 std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            }));

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [&tracking_get_status_callback](
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              EXPECT_EQ(states_in[1]->reader, "Reader B");
              ExpectInUse(*states_in[1]->current_state);
              EXPECT_EQ(states_in[1]->current_count, 1);

              tracking_get_status_callback.SetValue(std::move(callback));
            }));

    // The error given to the GetStatusChange() call at the end of the test to
    // finish it.
    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));

    // This unrecoverable failure should cause the tracker to drop its smart
    // card context and stop tracking.
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  // The first start() call, at t0.
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderInfoState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderInfoState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  // Wait until SmartCardReaderTracerImpl is in the Tracking state, waiting
  // for the GetStatusChange() call result.
  ASSERT_TRUE(tracking_get_status_callback.Wait());

  start_future.Clear();
  readers.reset();

  // The second start() call seemingly without progress in time.
  // Will just fulfill the request from the cache without restarting tracking.
  tracker_.Start(&observer_, start_future.GetCallback());

  readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderInfoState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderInfoState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  // End the test by simulating an error on the outstanding GetStatusChange.
  tracking_get_status_callback.Take().Run(
      SmartCardStatusChangeResult::NewError(SmartCardError::kNoService));

  ASSERT_TRUE(context_disconnected_.Wait());
}

// If Start() is called while tracking is already taking place and
// kMinRefreshInterval has elapsed since tracking has started, it should cause
// tracking to be restarted. ie, a new list of readers being fetched, etc.
TEST_F(SmartCardReaderTrackerImplTest, Restart) {
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;

  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Request what readers are currently available.
    mock_context_factory_.ExpectListReaders({"Reader A", "Reader B"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              ExpectUnaware(*states_in[1]);

              ReportStateOut(
                  std::move(callback),
                  CreateStateOut("Reader A", ReaderState::kEmpty),
                  CreateStateOut("Reader B", ReaderState::kInUse,
                                 /*changed=*/false,
                                 /*event_count=*/1,
                                 std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            }));

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [&tracking_get_status_callback](
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
            }));

    EXPECT_CALL(mock_context_factory_, Cancel)
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
    mock_context_factory_.ExpectListReaders({"Reader A", "Reader B"});

    // Since ListReaders did not return any reader unknown to the tracker,
    // it will now skip to waiting to be notified on any changes.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
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
            }));

    // The error given by the last GetStatusChange() call.
    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));

    // This unrecoverable failure should cause the tracker to drop its smart
    // card context and stop tracking.
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  // The first start() call, at t0.
  tracker_.Start(&observer_, start_future.GetCallback());

  OptionalReaderList readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderInfoState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderInfoState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  // Wait until SmartCardReaderTracerImpl is in the Tracking state, waiting
  // for the GetStatusChange() call result.
  ASSERT_TRUE(tracking_get_status_callback.Wait());
  task_environment_.FastForwardBy(
      SmartCardReaderTrackerImpl::kMinRefreshInterval);

  start_future.Clear();
  readers.reset();
  // The second start() call at t0 + kMinRefreshInterval.
  // Will make the tracker cancel the currently outstanding GetStatusChange()
  // request and restart from the ListReaders() call.
  tracker_.Start(&observer_, start_future.GetCallback());

  readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers,
              UnorderedElementsAre(
                  CreateReaderInfo("Reader A", ReaderInfoState::kEmpty),
                  CreateReaderInfo("Reader B", ReaderInfoState::kInUse,
                                   /*event_count=*/1,
                                   std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

  ASSERT_TRUE(context_disconnected_.Wait());
}

// Test that tracker will cancel its outstanding GetStatusChange() request and
// stop tracking when the last observer leaves.
TEST_F(SmartCardReaderTrackerImplTest, StopWhenTracking) {
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;

  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Request what readers are currently available.
    mock_context_factory_.ExpectListReaders({"Reader A"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kEmpty));
            }));

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [&tracking_get_status_callback](
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              // Handle that in the upcoming Cancel() call.
              tracking_get_status_callback.SetValue(std::move(callback));
            }));

    // When tracker.Stop() is called, the tracker should cancel the outstanding
    // GetStatusChange() request.
    EXPECT_CALL(mock_context_factory_, Cancel)
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
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  // Start()
  {
    TestFuture<OptionalReaderList> start_future;
    tracker_.Start(&observer_, start_future.GetCallback());
    ASSERT_TRUE(start_future.Wait());
  }

  // Wait until the tracker is in Tracking state, with an outstanding
  // GetStatusChange request.
  ASSERT_TRUE(tracking_get_status_callback.Wait());

  // Then Stop()
  tracker_.Stop(&observer_);

  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest, ReaderRemoved) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Request what readers are currently available.
    mock_context_factory_.ExpectListReaders({"Reader A"});

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              ExpectUnaware(*states_in[0]);

              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kEmpty));
            }));

    // Request to be notified of state changes on those readers.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(WithArgs<1, 2>(
            [](std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 1U);

              EXPECT_EQ(states_in[0]->reader, "Reader A");
              ExpectEmpty(*states_in[0]->current_state);
              EXPECT_EQ(states_in[0]->current_count, 0);

              // Reader A has been removed. It's now unknown.
              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kUnknown,
                                            /*changed=*/true));
            }));

    // Tracker will notify observers about this removal.
    EXPECT_CALL(observer_, OnReaderRemoved("Reader A"));

    // As there are no readers left, tracker should stop on its own and drop its
    // smart card context.
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  // Start()
  {
    TestFuture<OptionalReaderList> start_future;
    tracker_.Start(&observer_, start_future.GetCallback());
    ASSERT_TRUE(start_future.Wait());
  }

  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest, GetStatusChangeTimeoutInTracking) {
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Initial setup
    mock_context_factory_.ExpectListReaders({"Reader A"});
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(
            WithArg<2>([](SmartCardContext::GetStatusChangeCallback callback) {
              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kEmpty));
            }));

    // In Tracking state, GetStatusChange times out.
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(
            WithArg<2>([](SmartCardContext::GetStatusChangeCallback callback) {
              std::move(callback).Run(SmartCardStatusChangeResult::NewError(
                  SmartCardError::kTimeout));
            }));

    // After timeout, tracker should try to ListReaders again.
    // This time we return an error to end the test.
    mock_context_factory_.ExpectListReadersError(SmartCardError::kNoService);

    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));

    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());

  ASSERT_TRUE(start_future.Wait());
  ASSERT_TRUE(context_disconnected_.Wait());
}

TEST_F(SmartCardReaderTrackerImplTest, CancelFailedInTracking) {
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      tracking_get_status_callback;

  {
    InSequence s;

    EXPECT_CALL(mock_context_factory_, CreateContext);
    // Initial setup
    mock_context_factory_.ExpectListReaders({"Reader A"});
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(
            WithArg<2>([](SmartCardContext::GetStatusChangeCallback callback) {
              ReportStateOut(std::move(callback),
                             CreateStateOut("Reader A", ReaderState::kEmpty));
            }));

    // In Tracking state
    EXPECT_CALL(mock_context_factory_, GetStatusChange)
        .WillOnce(
            WithArg<2>([&tracking_get_status_callback](
                           SmartCardContext::GetStatusChangeCallback callback) {
              tracking_get_status_callback.SetValue(std::move(callback));
            }));

    // Cancel fails
    EXPECT_CALL(mock_context_factory_, Cancel)
        .WillOnce([](SmartCardContext::CancelCallback callback) {
          std::move(callback).Run(
              SmartCardResult::NewError(SmartCardError::kInternalError));
        });

    // GetStatusChange finally returns with an error to end the test.
    EXPECT_CALL(observer_, OnError(SmartCardError::kNoService));
    EXPECT_CALL(mock_context_factory_, ContextDisconnected())
        .WillOnce(InvokeFuture(context_disconnected_));
  }

  TestFuture<OptionalReaderList> start_future;
  tracker_.Start(&observer_, start_future.GetCallback());
  OptionalReaderList readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers, ElementsAre(CreateReaderInfo("Reader A",
                                                     ReaderInfoState::kEmpty)));

  // Tracking state
  ASSERT_TRUE(tracking_get_status_callback.Wait());
  task_environment_.FastForwardBy(
      SmartCardReaderTrackerImpl::kMinRefreshInterval);

  start_future.Clear();
  readers.reset();

  tracker_.Start(&observer_, start_future.GetCallback());
  // Should be fulfilled from cache
  readers = start_future.Take();
  ASSERT_TRUE(readers.has_value());
  EXPECT_THAT(*readers, ElementsAre(CreateReaderInfo("Reader A",
                                                     ReaderInfoState::kEmpty)));

  // End test by forcing error.
  tracking_get_status_callback.Take().Run(
      SmartCardStatusChangeResult::NewError(SmartCardError::kNoService));

  ASSERT_TRUE(context_disconnected_.Wait());
}

}  // namespace
