// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_ukm_reporter.h"

#include <memory>
#include <utility>

#include "base/test/simple_test_tick_clock.h"
#include "cc/metrics/predictor_jank_tracker.h"
#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"
#include "cc/metrics/ukm_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
const char kTestUrl[] = "https://example.com/foo";
const int64_t kTestSourceId = 100;
}  // namespace

class ScrollJankUkmReporterTest : public testing::Test {
 public:
  ScrollJankUkmReporterTest() = default;

  void SetUp() override {
    base_time_ = base::TimeTicks::Now();

    auto recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    test_ukm_recorder_ = recorder.get();
    ukm_manager_ = std::make_unique<UkmManager>(std::move(recorder));
    test_ukm_recorder_->UpdateSourceURL(kTestSourceId, GURL(kTestUrl));
    ukm_manager_->SetSourceId(kTestSourceId);

    scroll_jank_ukm_reporter_ = std::make_unique<ScrollJankUkmReporter>();
    scroll_jank_ukm_reporter_->set_ukm_manager(ukm_manager_.get());

    scroll_jank_dropped_frame_tracker_ =
        std::make_unique<ScrollJankDroppedFrameTracker>();
    scroll_jank_dropped_frame_tracker_->set_scroll_jank_ukm_reporter(
        scroll_jank_ukm_reporter_.get());
    scroll_jank_dropped_frame_tracker_->OnScrollStarted();

    predictor_jank_tracker_ = std::make_unique<PredictorJankTracker>();
    predictor_jank_tracker_->set_scroll_jank_ukm_reporter(
        scroll_jank_ukm_reporter_.get());
  }

  void ReportFramesToScrollJankDroppedFrameTracker(
      base::TimeTicks first_input_ts,
      base::TimeTicks last_input_ts,
      base::TimeTicks presentation_ts) {
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(base::TimeTicks(first_input_ts));
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
        /*is_inertial=*/false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
        /*delta=*/10.0f, first_input_ts, base::TimeTicks(), &tick_clock,
        /*trace_id=*/std::nullopt);
    scroll_jank_dropped_frame_tracker_->ReportLatestPresentationData(
        *event.get(), last_input_ts, presentation_ts, vsync_interval);
  }

  void ReportFramesToPredictorJankTracker(double delta,
                                          base::TimeTicks first_input_ts,
                                          base::TimeTicks presentation_ts) {
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(first_input_ts);
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
        /*is_inertial=*/false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
        /*delta=*/10.0f, first_input_ts, base::TimeTicks(), &tick_clock,
        /*trace_id=*/std::nullopt);
    predictor_jank_tracker_->ReportLatestScrollDelta(
        delta, presentation_ts, vsync_interval, event->trace_id());
  }

 protected:
  base::TimeTicks base_time_;
  std::unique_ptr<UkmManager> ukm_manager_;
  raw_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<ScrollJankUkmReporter> scroll_jank_ukm_reporter_;
  std::unique_ptr<ScrollJankDroppedFrameTracker>
      scroll_jank_dropped_frame_tracker_;
  std::unique_ptr<PredictorJankTracker> predictor_jank_tracker_;

 private:
  constexpr static base::TimeDelta vsync_interval = base::Milliseconds(16);
};

TEST_F(ScrollJankUkmReporterTest, NoJankUkmRecorded) {
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(ScrollJankUkmReporterTest, NoJankyFrames) {
  // Report one frame per vsync.
  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(111),
      base_time_ + base::Milliseconds(148));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(119),
      base_time_ + base::Milliseconds(127),
      base_time_ + base::Milliseconds(164));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->set_latest_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(164));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(), ukm::builders::Event_Scroll::kFrameCountName, 2);
  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_DelayedFrameCountName, 0);
}

TEST_F(ScrollJankUkmReporterTest, JankyFrames) {
  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(111),
      base_time_ + base::Milliseconds(148));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(119),
      base_time_ + base::Milliseconds(127),
      base_time_ + base::Milliseconds(196));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(135),
      base_time_ + base::Milliseconds(143),
      base_time_ + base::Milliseconds(228));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->set_latest_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(228));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(), ukm::builders::Event_Scroll::kFrameCountName, 3);
  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_DelayedFrameCountName, 2);
}

TEST_F(ScrollJankUkmReporterTest, NoMissedVsyncs) {
  // Report one frame per vsync.
  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(111),
      base_time_ + base::Milliseconds(148));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(119),
      base_time_ + base::Milliseconds(127),
      base_time_ + base::Milliseconds(164));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->set_latest_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(164));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(), ukm::builders::Event_Scroll::kVsyncCountName, 2);

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsMaxName, 0);
  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsSumName, 0);
}

TEST_F(ScrollJankUkmReporterTest, OneMissedVsync) {
  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(111),
      base_time_ + base::Milliseconds(148));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(119),
      base_time_ + base::Milliseconds(127),
      base_time_ + base::Milliseconds(164));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(135),
      base_time_ + base::Milliseconds(143),
      base_time_ + base::Milliseconds(196));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->set_latest_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(196));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(), ukm::builders::Event_Scroll::kVsyncCountName, 4);

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsMaxName, 1);
  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsSumName, 1);
}

TEST_F(ScrollJankUkmReporterTest, MultipleMissedVsyncs) {
  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(103),
      base_time_ + base::Milliseconds(148));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(119),
      base_time_ + base::Milliseconds(127),
      base_time_ + base::Milliseconds(196));

  ReportFramesToScrollJankDroppedFrameTracker(
      base_time_ + base::Milliseconds(135),
      base_time_ + base::Milliseconds(151),
      base_time_ + base::Milliseconds(228));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->set_latest_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(228));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(), ukm::builders::Event_Scroll::kVsyncCountName, 6);

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsMaxName, 2);
  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsSumName, 3);
}

TEST_F(ScrollJankUkmReporterTest, NoPredictorJank) {
  ReportFramesToPredictorJankTracker(10, base_time_,
                                     base_time_ + base::Milliseconds(103));
  ReportFramesToPredictorJankTracker(10, base_time_,
                                     base_time_ + base::Milliseconds(119));
  ReportFramesToPredictorJankTracker(10, base_time_,
                                     base_time_ + base::Milliseconds(135));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->UpdateLatestFrameAndEmitPredictorJank(
      base_time_ + base::Milliseconds(135));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto scroll_entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, scroll_entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      scroll_entries.back(),
      ukm::builders::Event_Scroll::kPredictorJankyFrameCountName, 0);

  auto predictor_entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_ScrollJank_PredictorJank::kEntryName);
  EXPECT_EQ(0u, predictor_entries.size());
}

TEST_F(ScrollJankUkmReporterTest, PredictorJankMissedVsync) {
  ReportFramesToPredictorJankTracker(10, base_time_,
                                     base_time_ + base::Milliseconds(103));
  ReportFramesToPredictorJankTracker(50, base_time_,
                                     base_time_ + base::Milliseconds(135));
  ReportFramesToPredictorJankTracker(10, base_time_,
                                     base_time_ + base::Milliseconds(151));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base_time_ + base::Milliseconds(103));
  scroll_jank_ukm_reporter_->UpdateLatestFrameAndEmitPredictorJank(
      base_time_ + base::Milliseconds(151));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kPredictorJankyFrameCountName, 1);

  auto predictor_entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_ScrollJank_PredictorJank::kEntryName);
  EXPECT_EQ(1u, predictor_entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::kMaxDeltaName, 44);

  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::
          kScrollUpdate_MissedVsync_FrameAboveJankyThreshold2Name,
      355);
  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::
          kScrollUpdate_NoMissedVsync_FrameAboveJankyThreshold2Name,
      0);
}

TEST_F(ScrollJankUkmReporterTest, PredictorJankNoMissedVsync) {
  ReportFramesToPredictorJankTracker(50, base::TimeTicks::Now(),
                                     base::TimeTicks::Now());
  ReportFramesToPredictorJankTracker(
      10, base::TimeTicks::Now(),
      base::TimeTicks::Now() + base::Milliseconds(16));
  ReportFramesToPredictorJankTracker(
      50, base::TimeTicks::Now(),
      base::TimeTicks::Now() + base::Milliseconds(32));

  scroll_jank_ukm_reporter_->set_first_frame_timestamp_for_testing(
      base::TimeTicks::Now());
  scroll_jank_ukm_reporter_->UpdateLatestFrameAndEmitPredictorJank(
      base::TimeTicks::Now() + base::Milliseconds(32));
  scroll_jank_ukm_reporter_->EmitScrollJankUkm();

  auto entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_Scroll::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::Event_Scroll::kPredictorJankyFrameCountName, 1);

  auto predictor_entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Event_ScrollJank_PredictorJank::kEntryName);
  EXPECT_EQ(1u, predictor_entries.size());

  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::kMaxDeltaName, 44);

  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::
          kScrollUpdate_MissedVsync_FrameAboveJankyThreshold2Name,
      0);
  test_ukm_recorder_->ExpectEntryMetric(
      predictor_entries.back(),
      ukm::builders::Event_ScrollJank_PredictorJank::
          kScrollUpdate_NoMissedVsync_FrameAboveJankyThreshold2Name,
      355);
}

}  // namespace cc
