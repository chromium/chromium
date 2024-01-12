// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_session.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr base::TimeDelta kEndDuration = base::Minutes(2);
constexpr base::TimeDelta kLongEndDuration =
    focus_mode_util::kMaximumDuration - base::Minutes(15);

}  // namespace

class FocusModeSessionTest : public testing::Test {
 public:
  FocusModeSessionTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FocusModeSessionTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests the normal flow.
TEST_F(FocusModeSessionTest, NormalCase) {
  base::Time start_time = base::Time::Now();
  base::Time expected_end_time = kEndDuration + start_time;

  // Create a FocusModeSession and verify that it is in the on state with the
  // right end time.
  FocusModeSession session{kEndDuration, expected_end_time};
  EXPECT_EQ(FocusModeSession::State::kOn, session.GetState(start_time));
  EXPECT_EQ(expected_end_time, session.end_time());

  // Verify it is still running.
  task_environment_.AdvanceClock(base::Minutes(1));
  EXPECT_EQ(FocusModeSession::State::kOn, session.GetState(base::Time::Now()));
  EXPECT_EQ(expected_end_time, session.end_time());

  // Verify that we enter the ending moment when the session duration elapses.
  task_environment_.AdvanceClock(base::Minutes(1));
  EXPECT_EQ(FocusModeSession::State::kEnding,
            session.GetState(base::Time::Now()));
  EXPECT_EQ(expected_end_time, session.end_time());

  // Verify that the ending moment terminates correctly.
  task_environment_.AdvanceClock(base::Minutes(1));
  EXPECT_EQ(FocusModeSession::State::kOff, session.GetState(base::Time::Now()));
}

// Tests that `GetSnapshot` gives the correct results.
TEST_F(FocusModeSessionTest, GetSnapshot) {
  base::Time start_time = base::Time::Now();
  base::Time expected_end_time = kEndDuration + start_time;
  FocusModeSession session{kEndDuration, expected_end_time};

  auto validate_snapshot = [&](const std::string& trace_name,
                               const base::Time current_time,
                               const FocusModeSession::State expected_state,
                               const base::TimeDelta expected_session_duration,
                               const base::TimeDelta expected_time_elapsed,
                               const base::TimeDelta expected_remaining_time,
                               const double expected_progress) {
    SCOPED_TRACE(trace_name);
    FocusModeSession::Snapshot session_snapshot =
        session.GetSnapshot(current_time);
    EXPECT_EQ(expected_state, session_snapshot.state);
    EXPECT_EQ(expected_session_duration, session_snapshot.session_duration);
    EXPECT_EQ(expected_time_elapsed, session_snapshot.time_elapsed);
    EXPECT_EQ(expected_remaining_time, session_snapshot.remaining_time);
  };

  validate_snapshot("Initial kOn snapshot", /*current_time=*/base::Time::Now(),
                    /*expected_state=*/FocusModeSession::State::kOn,
                    /*expected_session_duration=*/kEndDuration,
                    /*expected_time_elapsed=*/base::TimeDelta(),
                    /*expected_remaining_time=*/kEndDuration,
                    /*expected_progress=*/0);

  task_environment_.AdvanceClock(base::Minutes(1));
  validate_snapshot("Partway through session snapshot",
                    /*current_time=*/base::Time::Now(),
                    /*expected_state=*/FocusModeSession::State::kOn,
                    /*expected_session_duration=*/kEndDuration,
                    /*expected_time_elapsed=*/base::Minutes(1),
                    /*expected_remaining_time=*/base::Minutes(1),
                    /*expected_progress=*/0.5);

  task_environment_.AdvanceClock(base::Minutes(1));
  validate_snapshot("kEnding snapshot", /*current_time=*/base::Time::Now(),
                    /*expected_state=*/FocusModeSession::State::kEnding,
                    /*expected_session_duration=*/kEndDuration,
                    /*expected_time_elapsed=*/base::Minutes(2),
                    /*expected_remaining_time=*/base::TimeDelta(),
                    /*expected_progress=*/1);
}

// Tests that adding time to the ongoing session works correctly, and does not
// go past the maximum.
TEST_F(FocusModeSessionTest, ExtendSession) {
  base::Time start_time = base::Time::Now();
  base::Time expected_long_end_time = kLongEndDuration + start_time;
  FocusModeSession session{kLongEndDuration, expected_long_end_time};
  EXPECT_EQ(FocusModeSession::State::kOn, session.GetState(start_time));
  EXPECT_EQ(expected_long_end_time, session.end_time());

  // Extend the session duration with no special cases.
  session.ExtendSession(base::Time::Now());
  EXPECT_EQ(kLongEndDuration + focus_mode_util::kExtendDuration,
            session.session_duration());
  EXPECT_EQ(expected_long_end_time + focus_mode_util::kExtendDuration,
            session.end_time());

  // Extends the session duraton, reaching the max duration.
  session.ExtendSession(base::Time::Now());
  EXPECT_EQ(focus_mode_util::kMaximumDuration, session.session_duration());
  EXPECT_EQ(start_time + focus_mode_util::kMaximumDuration, session.end_time());

  // Try to extend the time when already at the maximum duration.
  session.ExtendSession(base::Time::Now());
  EXPECT_EQ(focus_mode_util::kMaximumDuration, session.session_duration());
  EXPECT_EQ(start_time + focus_mode_util::kMaximumDuration, session.end_time());
}

// Tests that a session that is marked for having a persistent ending doesn't
// become `kOff` even when passing the previously expected ending time.
TEST_F(FocusModeSessionTest, PersistEnding) {
  base::Time start_time = base::Time::Now();
  base::Time expected_end_time = kEndDuration + start_time;
  FocusModeSession session{kEndDuration, expected_end_time};
  EXPECT_EQ(FocusModeSession::State::kOn, session.GetState(start_time));

  // Advance the clock to enter the ending moment.
  task_environment_.AdvanceClock(base::Minutes(2));
  EXPECT_EQ(FocusModeSession::State::kEnding,
            session.GetState(base::Time::Now()));

  // Mark the session to have a persistent ending.
  session.set_persistent_ending();
  EXPECT_TRUE(session.persistent_ending());

  // Verify that even past the normal ending moment time, we are still in the
  // `kEnding` state.
  task_environment_.AdvanceClock(base::Minutes(1));
  EXPECT_EQ(FocusModeSession::State::kEnding,
            session.GetState(base::Time::Now()));

  // Tests that extending the session will calculate the new increase from the
  // current time. This will also unlock `persistent_ending_` (set it to false).
  base::Time extend_timestamp = base::Time::Now();
  session.ExtendSession(extend_timestamp);
  FocusModeSession::Snapshot session_snapshot =
      session.GetSnapshot(extend_timestamp);
  EXPECT_EQ(FocusModeSession::State::kOn, session_snapshot.state);
  EXPECT_EQ(kEndDuration + focus_mode_util::kExtendDuration,
            session_snapshot.session_duration);
  EXPECT_EQ(focus_mode_util::kExtendDuration, session_snapshot.remaining_time);
  EXPECT_EQ(extend_timestamp + focus_mode_util::kExtendDuration,
            session.end_time());
  EXPECT_FALSE(session.persistent_ending());

  // Test that the session can advance into the kOff state, and doesn't get
  // locked to `kEnding`.
  task_environment_.AdvanceClock(base::Minutes(11));
  EXPECT_EQ(FocusModeSession::State::kOff, session.GetState(base::Time::Now()));
}

}  // namespace ash
