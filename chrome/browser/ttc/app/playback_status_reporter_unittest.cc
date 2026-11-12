// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/playback_status_reporter.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

struct Report {
  base::TimeDelta playback_progress;
  bool playback_complete = false;
  int64_t generation_id = 0;

  bool operator==(const Report&) const = default;
};

class TtcPlaybackStatusReporterTest : public testing::Test {
 protected:
  PlaybackStatusReporter::SendReportCallback MakeCallback() {
    return base::BindLambdaForTesting([this](base::TimeDelta playback_progress,
                                             bool playback_complete,
                                             int64_t generation_id) {
      reports_.push_back({playback_progress, playback_complete, generation_id});
    });
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<Report> reports_;
};

TEST_F(TtcPlaybackStatusReporterTest, PeriodicTimerReportsAccumulatedProgress) {
  PlaybackStatusReporter reporter(/*generation_id=*/7, MakeCallback());

  // Timer ticks before any audio has played do not emit empty reports.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(reports_.empty());

  // Progress is accumulated rather than reported synchronously.
  reporter.OnPlaybackProgress(base::Milliseconds(120),
                              /*did_fully_drain_queue=*/false);
  EXPECT_TRUE(reports_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(120), false, 7}));

  // Repeated ticks without new progress are skipped.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(reports_.size(), 1u);

  reporter.OnPlaybackProgress(base::Milliseconds(80),
                              /*did_fully_drain_queue=*/false);
  reporter.OnPlaybackProgress(base::Milliseconds(50),
                              /*did_fully_drain_queue=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(reports_.size(), 2u);
  EXPECT_EQ(reports_[1], (Report{base::Milliseconds(250), false, 7}));

  reporter.OnGenerationCompleted(/*is_playing=*/false);
  ASSERT_EQ(reports_.size(), 3u);
  EXPECT_EQ(reports_[2], (Report{base::Milliseconds(250), true, 7}));
}

TEST_F(TtcPlaybackStatusReporterTest,
       GenerationCompletedWhilePlayingWaitsForDrain) {
  PlaybackStatusReporter reporter(/*generation_id=*/11, MakeCallback());

  reporter.OnPlaybackProgress(base::Milliseconds(100),
                              /*did_fully_drain_queue=*/false);
  reporter.OnGenerationCompleted(/*is_playing=*/true);
  EXPECT_TRUE(reports_.empty());

  // Still playing queued audio.
  reporter.OnPlaybackProgress(base::Milliseconds(100),
                              /*did_fully_drain_queue=*/false);
  EXPECT_TRUE(reports_.empty());

  // Queue finishes draining; completion is reported immediately and the timer
  // stops.
  reporter.OnPlaybackProgress(base::Milliseconds(50),
                              /*did_fully_drain_queue=*/true);
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(250), true, 11}));

  // Further calls or timer ticks after completion are ignored.
  reporter.OnPlaybackProgress(base::Milliseconds(50),
                              /*did_fully_drain_queue=*/true);
  reporter.OnInterrupted();
  reporter.OnGenerationCompleted(/*is_playing=*/false);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest,
       GenerationCompletedWhenAlreadyDrainedReportsImmediately) {
  PlaybackStatusReporter reporter(/*generation_id=*/15, MakeCallback());

  reporter.OnPlaybackProgress(base::Milliseconds(90),
                              /*did_fully_drain_queue=*/true);
  EXPECT_TRUE(reports_.empty());

  reporter.OnGenerationCompleted(/*is_playing=*/false);
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(90), true, 15}));

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest, InterruptedReportsCompletionImmediately) {
  PlaybackStatusReporter reporter(/*generation_id=*/21, MakeCallback());

  reporter.OnPlaybackProgress(base::Milliseconds(140),
                              /*did_fully_drain_queue=*/false);
  reporter.OnInterrupted();
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(140), true, 21}));

  reporter.OnPlaybackProgress(base::Milliseconds(60),
                              /*did_fully_drain_queue=*/true);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest,
       PlaybackErrorReportedCompletionImmediately) {
  PlaybackStatusReporter reporter(/*generation_id=*/25, MakeCallback());

  reporter.OnPlaybackProgress(base::Milliseconds(150),
                              /*did_fully_drain_queue=*/false);
  reporter.OnPlaybackError();
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(150), true, 25}));

  reporter.OnPlaybackProgress(base::Milliseconds(60),
                              /*did_fully_drain_queue=*/true);
  reporter.OnGenerationCompleted(/*is_playing=*/false);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest, DestructorDoesNotSendReport) {
  {
    auto reporter = std::make_unique<PlaybackStatusReporter>(
        /*generation_id=*/30, MakeCallback());
    reporter->OnPlaybackProgress(base::Milliseconds(50),
                                 /*did_fully_drain_queue=*/false);
  }
  EXPECT_TRUE(reports_.empty());

  {
    auto reporter = std::make_unique<PlaybackStatusReporter>(
        /*generation_id=*/31, MakeCallback());
    reporter->OnPlaybackProgress(base::Milliseconds(50),
                                 /*did_fully_drain_queue=*/false);
    task_environment_.FastForwardBy(base::Milliseconds(200));
    ASSERT_EQ(reports_.size(), 1u);
    EXPECT_EQ(reports_[0], (Report{base::Milliseconds(50), false, 31}));
  }
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest, CompleteIfNeededSendsCompletion) {
  PlaybackStatusReporter reporter(/*generation_id=*/32, MakeCallback());
  reporter.OnPlaybackProgress(base::Milliseconds(70),
                              /*did_fully_drain_queue=*/false);
  EXPECT_TRUE(reports_.empty());

  reporter.CompleteIfNeeded();
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(70), true, 32}));

  // Calling CompleteIfNeeded again is a no-op.
  reporter.CompleteIfNeeded();
  EXPECT_EQ(reports_.size(), 1u);
}

TEST_F(TtcPlaybackStatusReporterTest, RespectsReportIntervalFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kTtc, {{"playback_status_report_interval", "50ms"}});

  PlaybackStatusReporter reporter(/*generation_id=*/99, MakeCallback());
  reporter.OnPlaybackProgress(base::Milliseconds(40),
                              /*did_fully_drain_queue=*/false);

  task_environment_.FastForwardBy(base::Milliseconds(49));
  EXPECT_TRUE(reports_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(reports_.size(), 1u);
  EXPECT_EQ(reports_[0], (Report{base::Milliseconds(40), false, 99}));

  reporter.OnGenerationCompleted(/*is_playing=*/false);
}

}  // namespace
}  // namespace ttc
