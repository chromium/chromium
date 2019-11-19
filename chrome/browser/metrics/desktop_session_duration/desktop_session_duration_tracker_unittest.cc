// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

const base::TimeDelta kZeroTime = base::TimeDelta::FromSeconds(0);
const base::TimeDelta kInactivityTimeoutForTesting =
    base::TimeDelta::FromSeconds(1);

// Mock class for |DesktopSessionDurationTracker| for testing.
class MockDesktopSessionDurationTracker : public DesktopSessionDurationTracker {
 public:
  MockDesktopSessionDurationTracker() {}

  bool is_timeout() const { return time_out_; }

  using DesktopSessionDurationTracker::OnAudioStart;
  using DesktopSessionDurationTracker::OnAudioEnd;

 protected:
  void OnTimerFired() override {
    DesktopSessionDurationTracker::OnTimerFired();
    time_out_ = true;
  }

 private:
  bool time_out_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDesktopSessionDurationTracker);
};

// Mock class for |DesktopSessionDurationTracker::Observer| for testing.
class MockDesktopSessionObserver
    : public metrics::DesktopSessionDurationTracker::Observer {
 public:
  MockDesktopSessionObserver() {}

  int session_started_count() const { return session_started_count_; }
  int session_ended_count() const { return session_ended_count_; }

 protected:
  // metrics::DesktopSessionDurationTracker::Observer:
  void OnSessionStarted(base::TimeTicks session_start) override {
    session_started_count_ = true;
  }
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override {
    session_ended_count_ = true;
  }

 private:
  int session_started_count_ = false;
  int session_ended_count_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDesktopSessionObserver);
};

class DesktopSessionDurationTrackerTest : public testing::Test {
 public:
  DesktopSessionDurationTrackerTest() {}

  void SetUp() override {
    metrics::DesktopSessionDurationTracker::Initialize();
    instance_.SetInactivityTimeoutForTesting(kInactivityTimeoutForTesting);
  }

  void TearDown() override {
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  void ExpectTotalDuration(base::TimeDelta duration) {
    histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
    base::Bucket bucket =
        histogram_tester_.GetAllSamples("Session.TotalDuration")[0];
    int max_expected_value = duration.InMilliseconds();
    EXPECT_LE(bucket.min, max_expected_value);
  }

  base::HistogramTester histogram_tester_;
  MockDesktopSessionDurationTracker instance_;
  MockDesktopSessionObserver observer_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionDurationTrackerTest);
};

}  // namespace

TEST_F(DesktopSessionDurationTrackerTest, TestVisibility) {
  // The browser becomes visible but it shouldn't start the session.
  instance_.OnVisibilityChanged(true, kZeroTime);
  EXPECT_FALSE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnUserEvent();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Even if there is a recent user event visibility change should end the
  // session.
  instance_.OnUserEvent();
  instance_.OnUserEvent();
  instance_.OnVisibilityChanged(false, kZeroTime);
  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);

  // For the second time only visibility change should start the session.
  instance_.OnVisibilityChanged(true, kZeroTime);
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
  instance_.OnVisibilityChanged(false, kZeroTime);
  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 2);
}

TEST_F(DesktopSessionDurationTrackerTest, TestUserEvent) {
  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // User event doesn't go through if nothing is visible.
  instance_.OnUserEvent();
  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnVisibilityChanged(true, kZeroTime);
  instance_.OnUserEvent();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance_.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
}

TEST_F(DesktopSessionDurationTrackerTest, TestAudioEvent) {
  instance_.OnVisibilityChanged(true, kZeroTime);
  instance_.OnAudioStart();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_TRUE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnVisibilityChanged(false, kZeroTime);
  EXPECT_TRUE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_TRUE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnAudioEnd();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance_.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_FALSE(instance_.is_audio_playing());
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
}

TEST_F(DesktopSessionDurationTrackerTest, TestInputTimeoutDiscount) {
  instance_.OnVisibilityChanged(true, kZeroTime);
  base::TimeTicks before_session_start = base::TimeTicks::Now();
  instance_.OnUserEvent();  // This should start the session
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance_.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }
  base::TimeTicks after_session_end = base::TimeTicks::Now();

  ExpectTotalDuration(after_session_end - before_session_start -
                      kInactivityTimeoutForTesting);
}

TEST_F(DesktopSessionDurationTrackerTest, TestVisibilityTimeoutDiscount) {
  instance_.OnVisibilityChanged(true, kZeroTime);
  base::TimeTicks before_session_start = base::TimeTicks::Now();
  instance_.OnUserEvent();  // This should start the session
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Sleep a little while.
  base::TimeDelta kDelay = base::TimeDelta::FromSeconds(2);
  while (true) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - before_session_start;
    if (elapsed >= kDelay)
      break;
    base::PlatformThread::Sleep(kDelay);
  }

  // End the session via visibility change.
  instance_.OnVisibilityChanged(false, kDelay);
  base::TimeTicks after_session_end = base::TimeTicks::Now();

  ExpectTotalDuration(after_session_end - before_session_start - kDelay);
}

TEST_F(DesktopSessionDurationTrackerTest, TestObserver) {
  instance_.AddObserver(&observer_);

  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_EQ(observer_.session_started_count(), 0);
  EXPECT_EQ(observer_.session_ended_count(), 0);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnVisibilityChanged(true, kZeroTime);
  instance_.OnUserEvent();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_EQ(observer_.session_ended_count(), 0);
  EXPECT_EQ(observer_.session_started_count(), 1);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance_.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_EQ(observer_.session_started_count(), 1);
  EXPECT_EQ(observer_.session_ended_count(), 1);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
}

TEST_F(DesktopSessionDurationTrackerTest, TestNoDoubleEndSession) {
  instance_.AddObserver(&observer_);

  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_EQ(observer_.session_started_count(), 0);
  EXPECT_EQ(observer_.session_ended_count(), 0);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  instance_.OnVisibilityChanged(true, kZeroTime);
  instance_.OnUserEvent();
  EXPECT_TRUE(instance_.in_session());
  EXPECT_TRUE(instance_.is_visible());
  EXPECT_EQ(observer_.session_ended_count(), 0);
  EXPECT_EQ(observer_.session_started_count(), 1);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 0);

  // Simulate Chrome being hidden. This should end the session.
  instance_.OnVisibilityChanged(false, kZeroTime);
  EXPECT_FALSE(instance_.in_session());
  EXPECT_FALSE(instance_.is_visible());
  EXPECT_EQ(observer_.session_ended_count(), 1);
  EXPECT_EQ(observer_.session_started_count(), 1);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);

  // When the timeout expires, observers shouldn't be notified again and the
  // histogram shouldn't be recorded again.
  base::PlatformThread::Sleep(kInactivityTimeoutForTesting * 2);
  EXPECT_EQ(observer_.session_ended_count(), 1);
  EXPECT_EQ(observer_.session_started_count(), 1);
  histogram_tester_.ExpectTotalCount("Session.TotalDuration", 1);
}

}  // namespace metrics
