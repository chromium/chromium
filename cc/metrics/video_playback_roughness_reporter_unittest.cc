// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/video_playback_roughness_reporter.h"

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using VideoFrame = media::VideoFrame;
using VideoFrameMetadata = media::VideoFrameMetadata;

namespace cc {

class VideoPlaybackRoughnessReporterTest : public ::testing::Test {
 protected:
  std::unique_ptr<VideoPlaybackRoughnessReporter> reporter_;
  base::TimeTicks time_;
  int token_ = 0;

  template <class T>
  void SetReportingCallabck(T cb) {
    reporter_ = std::make_unique<VideoPlaybackRoughnessReporter>(
        base::BindLambdaForTesting(cb));
  }

  VideoPlaybackRoughnessReporter* reporter() {
    DCHECK(reporter_);
    return reporter_.get();
  }

  scoped_refptr<VideoFrame> MakeFrame(base::TimeDelta duration,
                                      int frame_size = 100) {
    scoped_refptr<VideoFrame> result = media::VideoFrame::CreateColorFrame(
        gfx::Size(frame_size, frame_size), 0x80, 0x80, 0x80, base::TimeDelta());
    result->metadata().wallclock_frame_duration = duration;
    return result;
  }

  ::testing::AssertionResult CheckSizes() {
    size_t max_frames =
        2 * size_t{VideoPlaybackRoughnessReporter::kMaxWindowSize};
    if (reporter()->frames_.size() > max_frames)
      return ::testing::AssertionFailure();

    constexpr int max_worst_windows_size =
        1 + VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit *
                (100 - VideoPlaybackRoughnessReporter::kPercentileToSubmit) /
                100;
    if (reporter()->worst_windows_.size() > max_worst_windows_size)
      return ::testing::AssertionFailure()
             << "windows " << reporter()->worst_windows_.size();
    return ::testing::AssertionSuccess();
  }

  void NormalRun(double fps,
                 double hz,
                 std::vector<int> cadence,
                 int frames,
                 int frame_size = 100) {
    base::TimeDelta vsync = base::Seconds(1 / hz);
    base::TimeDelta ideal_duration = base::Seconds(1 / fps);
    for (int idx = 0; idx < frames; idx++) {
      int frame_cadence = cadence[idx % cadence.size()];
      base::TimeDelta duration = vsync * frame_cadence;
      auto frame = MakeFrame(ideal_duration, frame_size);
      reporter()->FrameSubmitted(token_, *frame, vsync);
      reporter()->FramePresented(token_++, time_, true);
      reporter()->ProcessFrameWindow();
      time_ += duration;
    }
  }

  void BatchPresentationRun(double fps,
                            double hz,
                            std::vector<int> cadence,
                            int frames) {
    base::TimeDelta vsync = base::Seconds(1 / hz);
    base::TimeDelta ideal_duration = base::Seconds(1 / fps);
    constexpr int batch_size = 3;
    for (int idx = 0; idx < frames; idx++) {
      auto frame = MakeFrame(ideal_duration);
      reporter()->FrameSubmitted(idx, *frame, vsync);
      if (idx % batch_size == batch_size - 1) {
        for (int i = batch_size - 1; i >= 0; i--) {
          int presented_idx = idx - i;
          int frame_cadence = cadence[presented_idx % cadence.size()];
          base::TimeDelta duration = vsync * frame_cadence;
          reporter()->FramePresented(presented_idx, time_, true);
          time_ += duration;
        }
      }

      reporter()->ProcessFrameWindow();
    }
  }

  void FreezingRun(double fps,
                   double hz,
                   std::vector<int> cadence,
                   int frames,
                   int frame_size = 100,
                   int freeze_on_frame = 50,
                   int frozen_vsyncs = 10) {
    base::TimeDelta vsync = base::Seconds(1 / hz);
    base::TimeDelta ideal_duration = base::Seconds(1 / fps);
    for (int idx = 0; idx < frames; idx++) {
      int frame_cadence = cadence[idx % cadence.size()];
      base::TimeDelta duration = vsync * frame_cadence;
      auto frame = MakeFrame(ideal_duration, frame_size);
      reporter()->FrameSubmitted(token_, *frame, vsync);
      reporter()->FramePresented(token_++, time_, true);
      reporter()->ProcessFrameWindow();
      if (idx == freeze_on_frame)
        time_ += duration * frozen_vsyncs;
      else
        time_ += duration;
    }
  }
};

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase24fps) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_EQ(measurement.refresh_rate_hz, 60);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 5.9, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 10;
  NormalRun(fps, 60, {2, 3}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase24fpsOn120Hz) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_EQ(measurement.refresh_rate_hz, 120);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 0.0, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 10;
  NormalRun(fps, 120, {5}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase30fps) {
  int call_count = 0;
  int fps = 30;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 0.0, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {2}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

// This cadence pattern was used in the small user study and was found
// to be perceived by participants as not as good as ideal 30fps playback but
// better than the pattern from UserStudyBad.
// The main characteristic of this test is that cadence breaks by having a frame
// shown only once, but the very next frame is being shown 3 times thus
// fixing the synchronization.
TEST_F(VideoPlaybackRoughnessReporterTest, UserStudyOkay) {
  int call_count = 0;
  int fps = 30;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 4.3, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {2, 2, 2, 2, 2, 2, 1, 3, 2, 2, 2, 2, 2, 2, 2},
            frames_to_run);
  EXPECT_EQ(call_count, 1);
}

// This cadence pattern was used in the small user study and was found
// to be perceived as worst of all options in the study.
// The main characteristic of this test is that cadence breaks by having a frame
// shown only once, and it takes 2 more frames for a frame that is shown 3 times
// thus fixing the synchronization.
TEST_F(VideoPlaybackRoughnessReporterTest, UserStudyBad) {
  int call_count = 0;
  int fps = 30;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 7.46, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {2, 2, 2, 2, 2, 1, 2, 2, 3, 2, 2, 2, 2, 2, 2},
            frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, Glitchy24fps) {
  int call_count = 0;
  int fps = 24;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 14.8, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {2, 3, 1, 3, 2, 4, 2, 3, 2, 3, 3, 3}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase60fps) {
  int call_count = 0;
  int fps = 60;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 0.0, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {1}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

TEST_F(VideoPlaybackRoughnessReporterTest, BestCase50fps) {
  int call_count = 0;
  int fps = 50;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 8.1, 01);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {1, 1, 1, 1, 2}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

// Test that we understand the roughness algorithm by checking that we can
// get any result we need.
TEST_F(VideoPlaybackRoughnessReporterTest, PredictableRoughnessValue) {
  int fps = 12;
  int frames_in_window = fps;
  int call_count = 0;
  double intended_roughness = 4.2;
  base::TimeDelta vsync = base::Seconds(1.0 / fps);
  // Calculating the error value that needs to be injected into one frame
  // in order to get desired roughness.
  base::TimeDelta error = base::Milliseconds(
      std::sqrt(intended_roughness * intended_roughness * frames_in_window));

  auto callback =
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(frames_in_window, measurement.frames);
        ASSERT_NEAR(measurement.roughness, intended_roughness, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.0, 0.1);
        call_count++;
      };
  SetReportingCallabck(callback);
  int token = 0;
  int win_count = 50;
  for (int win_idx = 0; win_idx < win_count; win_idx++) {
    for (int frame_idx = 0; frame_idx < frames_in_window; frame_idx++) {
      base::TimeTicks time;
      time += token * vsync;
      if (frame_idx == frames_in_window - 1)
        time += error;

      auto frame = MakeFrame(vsync);
      reporter()->FrameSubmitted(token, *frame, vsync);
      reporter()->FramePresented(token++, time, true);
      reporter()->ProcessFrameWindow();
    }
  }
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that the reporter indeed takes 95% worst window.
TEST_F(VideoPlaybackRoughnessReporterTest, TakingPercentile) {
  int token = 0;
  int fps = 12;
  int frames_in_window = fps;
  int call_count = 0;
  int win_count = 100;
  base::TimeDelta vsync = base::Seconds(1.0 / fps);
  std::vector<double> targets;
  targets.reserve(win_count);
  for (int i = 0; i < win_count; i++)
    targets.push_back(i * 0.1);
  double expected_roughness =
      VideoPlaybackRoughnessReporter::kPercentileToSubmit * 0.1;
  std::mt19937 rnd(1);
  std::shuffle(targets.begin(), targets.end(), rnd);

  auto callback =
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(frames_in_window, measurement.frames);
        ASSERT_NEAR(measurement.roughness, expected_roughness, 0.05);
        call_count++;
      };
  SetReportingCallabck(callback);

  for (int win_idx = 0; win_idx < win_count; win_idx++) {
    double roughness = targets[win_idx];
    // Calculating the error value that needs to be injected into one frame
    // in order to get desired roughness.
    base::TimeDelta error =
        base::Milliseconds(std::sqrt(roughness * roughness * frames_in_window));

    for (int frame_idx = 0; frame_idx < frames_in_window; frame_idx++) {
      base::TimeTicks time;
      time += token * vsync;
      if (frame_idx == frames_in_window - 1)
        time += error;

      auto frame = MakeFrame(vsync);
      reporter()->FrameSubmitted(token, *frame, vsync);
      reporter()->FramePresented(token++, time, true);
      reporter()->ProcessFrameWindow();
    }
  }
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that even if no windows can be reported due to unstable presentation
// feedback, the reporter still doesn't run out of memory.
TEST_F(VideoPlaybackRoughnessReporterTest, LongRunWithoutWindows) {
  int call_count = 0;
  base::TimeDelta vsync = base::Milliseconds(1);
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        call_count++;
      });
  for (int i = 0; i < 10000; i++) {
    auto frame = MakeFrame(vsync);
    reporter()->FrameSubmitted(i, *frame, vsync);
    if (i % 2 == 0)
      reporter()->FramePresented(i, base::TimeTicks() + i * vsync, true);
    reporter()->ProcessFrameWindow();
    ASSERT_TRUE(CheckSizes());
  }
  EXPECT_EQ(call_count, 0);
}

// Test that the reporter is no spooked by FramePresented() on unknown frame
// tokens.
TEST_F(VideoPlaybackRoughnessReporterTest, PresentingUnknownFrames) {
  int call_count = 0;
  base::TimeDelta vsync = base::Milliseconds(1);
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        call_count++;
      });
  for (int i = 0; i < 10000; i++) {
    auto frame = MakeFrame(vsync);
    reporter()->FrameSubmitted(i, *frame, vsync);
    reporter()->FramePresented(i + 100000, base::TimeTicks() + i * vsync, true);
    reporter()->ProcessFrameWindow();
    ASSERT_TRUE(CheckSizes());
  }
  EXPECT_EQ(call_count, 0);
}

// Test that the reporter is ignoring frames with unreliable
// presentation timestamp.
TEST_F(VideoPlaybackRoughnessReporterTest, IgnoringUnreliableTimings) {
  int call_count = 0;
  base::TimeDelta vsync = base::Milliseconds(1);
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        call_count++;
      });
  for (int i = 0; i < 10000; i++) {
    auto frame = MakeFrame(vsync);
    reporter()->FrameSubmitted(i, *frame, vsync);
    reporter()->FramePresented(i, base::TimeTicks() + i * vsync, false);
    reporter()->ProcessFrameWindow();
    ASSERT_TRUE(CheckSizes());
  }
  EXPECT_EQ(call_count, 0);
}

// Test that Reset() causes reporting if there is sufficient number of windows
// accumulated.
TEST_F(VideoPlaybackRoughnessReporterTest, ReportingInReset) {
  int call_count = 0;
  int fps = 60;
  auto callback =
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        call_count++;
      };
  SetReportingCallabck(callback);

  // Set number of frames insufficient for reporting in Reset()
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit * fps - 1;
  NormalRun(fps, 60, {1}, frames_to_run);
  // No calls since, not enough windows were reported
  EXPECT_EQ(call_count, 0);

  // Reset the reporter, still no calls
  reporter()->Reset();
  EXPECT_EQ(call_count, 0);

  // Set number of frames sufficient for reporting in Reset()
  frames_to_run =
      VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit * fps + 1;
  NormalRun(fps, 60, {1}, frames_to_run);

  // No calls since, not enough windows were reported
  EXPECT_EQ(call_count, 0);

  // A window should be reported in the Reset()
  reporter()->Reset();
  EXPECT_EQ(call_count, 1);
}

// Test that a change of display refresh rate or frame size causes reporting
// iff there is sufficient number of windows accumulated.
TEST_F(VideoPlaybackRoughnessReporterTest, ReportingAfterParameterChange) {
  struct Report {
    int hz;
    int height;
    double roughness;
  };
  std::vector<Report> reports;
  int fps = 60;
  auto callback =
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        reports.push_back({measurement.refresh_rate_hz,
                           measurement.frame_size.height(),
                           measurement.roughness});
      };
  SetReportingCallabck(callback);

  int frames_to_run =
      (VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit - 1) * fps + 3;
  NormalRun(fps, 59, {1}, frames_to_run, 480);
  ASSERT_TRUE(reports.empty());

  frames_to_run =
      (VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit + 1) * fps + 3;
  NormalRun(fps, 60, {1}, frames_to_run, 480);
  // Check that if parameters change after only a few windows, nothing gets
  // reported.
  ASSERT_TRUE(reports.empty());

  frames_to_run =
      (VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit + 1) * fps + 3;
  NormalRun(fps, 120, {2}, frames_to_run, 481);

  // Check that if parameters change after sufficient number of windows
  // roughness is reported. The second report is done normally after max
  // number of windows is seen.
  ASSERT_EQ(reports.size(), 2u);
  EXPECT_EQ(reports[0].hz, 60);
  EXPECT_EQ(reports[0].height, 480);
  EXPECT_EQ(reports[0].roughness, 0.0);
  EXPECT_EQ(reports[1].hz, 120);
  EXPECT_EQ(reports[1].height, 481);
  EXPECT_EQ(reports[1].roughness, 0.0);
}

// Test that reporting works even if frame presentation signal come out of
// order.
TEST_F(VideoPlaybackRoughnessReporterTest, BatchPresentation) {
  int call_count = 0;
  int fps = 60;

  // Try 60 fps
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 0.0, 0.1);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 10;
  BatchPresentationRun(fps, 60, {1}, frames_to_run);
  EXPECT_EQ(call_count, 1);

  // Try 24fps
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 5.9, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0, 0.01);
        call_count++;
      });
  fps = 24;
  frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 10;
  BatchPresentationRun(fps, 60, {2, 3}, frames_to_run);
  EXPECT_EQ(call_count, 2);
}

TEST_F(VideoPlaybackRoughnessReporterTest, Freezing30fps) {
  int call_count = 0;
  int fps = 30;
  SetReportingCallabck(
      [&](const VideoPlaybackRoughnessReporter::Measurement& measurement) {
        ASSERT_EQ(measurement.frames, fps);
        ASSERT_NEAR(measurement.duration.InMillisecondsF(), 1000.0, 1.0);
        ASSERT_NEAR(measurement.roughness, 0.0, 0.1);
        ASSERT_NEAR(measurement.freezing.InSecondsF(), 0.25, 0.05);
        call_count++;
      });
  int frames_to_run =
      VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit * fps + 1;
  FreezingRun(fps, 60, {2}, frames_to_run);
  EXPECT_EQ(call_count, 1);
}

}  // namespace cc
