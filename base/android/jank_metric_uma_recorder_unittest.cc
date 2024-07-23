// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jank_metric_uma_recorder.h"

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "jank_metric_uma_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace base::android {
namespace {

jlongArray GenerateJavaLongArray(JNIEnv* env, span<const int64_t> longs) {
  ScopedJavaLocalRef<jlongArray> java_long_array = ToJavaLongArray(env, longs);
  return java_long_array.Release();
}

jintArray GenerateJavaIntArray(JNIEnv* env, span<const int> ints) {
  ScopedJavaLocalRef<jintArray> java_int_array = ToJavaIntArray(env, ints);
  return java_int_array.Release();
}

// Durations are received in nanoseconds, but are recorded to UMA in
// milliseconds.
const int64_t kDurations[] = {
    1'000'000,   // 1ms
    2'000'000,   // 2ms
    30'000'000,  // 30ms
    10'000'000,  // 10ms
    60'000'000,  // 60ms
    1'000'000,   // 1ms
    1'000'000,   // 1ms
    20'000'000,  // 20ms
};

const int kMissedVsyncs[] = {
    0, 0, 2, 0, 1, 0, 0, 0,
};

static_assert(std::size(kDurations) == std::size(kMissedVsyncs));
const size_t kNumFrames = std::size(kDurations);

struct ScrollTestCase {
  JankScenario scenario;
  std::string test_name;
  int num_frames;
  std::string suffix;
};

}  // namespace

TEST(JankMetricUMARecorder, TestUMARecording) {

  JNIEnv* env = AttachCurrentThread();

  jlongArray java_durations = GenerateJavaLongArray(env, kDurations);

  jintArray java_missed_vsyncs = GenerateJavaIntArray(env, kMissedVsyncs);

  const int kMinScenario = static_cast<int>(JankScenario::PERIODIC_REPORTING);
  const int kMaxScenario = static_cast<int>(JankScenario::MAX_VALUE);
  // keep one histogram tester outside to ensure that each histogram is a
  // different one rather than just the same string over and over.
  HistogramTester complete_histogram_tester;
  size_t total_histograms = 0;
  for (int i = kMinScenario; i < kMaxScenario; ++i) {
    if ((i == static_cast<int>(JankScenario::WEBVIEW_SCROLLING)) ||
        (i == static_cast<int>(JankScenario::FEED_SCROLLING))) {
      continue;
    }
    // HistogramTester takes a snapshot of currently incremented counters so
    // everything is scoped to just this iteration of the for loop.
    HistogramTester histogram_tester;

    RecordJankMetrics(
        env,
        /* java_durations_ns= */
        base::android::JavaParamRef<jlongArray>(env, java_durations),
        /* java_missed_vsyncs = */
        base::android::JavaParamRef<jintArray>(env, java_missed_vsyncs),
        /* java_reporting_interval_start_time = */ 0,
        /* java_reporting_interval_duration = */ 1000,
        /* java_scenario_enum = */ i);

    const std::string kDurationName =
        GetAndroidFrameTimelineDurationHistogramName(
            static_cast<JankScenario>(i));
    const std::string kJankyName =
        GetAndroidFrameTimelineJankHistogramName(static_cast<JankScenario>(i));

    // Only one Duration and one Jank scenario should be incremented.
    base::HistogramTester::CountsMap count_map =
        histogram_tester.GetTotalCountsForPrefix("Android.FrameTimelineJank.");
    EXPECT_EQ(count_map.size(), 2ul);
    EXPECT_EQ(count_map[kDurationName], 8) << kDurationName;
    EXPECT_EQ(count_map[kJankyName], 8) << kJankyName;
    // And we should be two more then last iteration, but don't do any other
    // verification because each iteration will do their own.
    base::HistogramTester::CountsMap total_count_map =
        complete_histogram_tester.GetTotalCountsForPrefix(
            "Android.FrameTimelineJank.");
    EXPECT_EQ(total_count_map.size(), total_histograms + 2);
    total_histograms += 2;

    EXPECT_THAT(histogram_tester.GetAllSamples(kDurationName),
                ElementsAre(Bucket(1, 3), Bucket(2, 1), Bucket(10, 1),
                            Bucket(20, 1), Bucket(29, 1), Bucket(57, 1)))
        << kDurationName;
    EXPECT_THAT(histogram_tester.GetAllSamples(kJankyName),
                ElementsAre(Bucket(FrameJankStatus::kJanky, 2),
                            Bucket(FrameJankStatus::kNonJanky, 6)))
        << kJankyName;
  }
}

TEST(JankMetricUMARecorder, TestWebviewScrollingScenario) {
  JNIEnv* env = AttachCurrentThread();

  jlongArray java_durations = GenerateJavaLongArray(env, kDurations);
  jintArray java_missed_vsyncs = GenerateJavaIntArray(env, kMissedVsyncs);

  const int scenario = static_cast<int>(JankScenario::WEBVIEW_SCROLLING);
  HistogramTester histogram_tester;
  RecordJankMetrics(
      env,
      /* java_durations_ns= */
      base::android::JavaParamRef<jlongArray>(env, java_durations),
      /* java_missed_vsyncs = */
      base::android::JavaParamRef<jintArray>(env, java_missed_vsyncs),
      /* java_reporting_interval_start_time = */ 0,
      /* java_reporting_interval_duration = */ 1000, scenario);

  const std::string kDurationName =
      "Android.FrameTimelineJank.Duration.WebviewScrolling";
  const std::string kJankyName =
      "Android.FrameTimelineJank.FrameJankStatus.WebviewScrolling";
  histogram_tester.ExpectTotalCount(kDurationName, 0);
  histogram_tester.ExpectTotalCount(kJankyName, 0);
}

TEST(JankMetricUMARecorder, TestCombinedWebviewScrollingScenario) {
  JNIEnv* env = AttachCurrentThread();

  jlongArray java_durations = GenerateJavaLongArray(env, kDurations);
  jintArray java_missed_vsyncs = GenerateJavaIntArray(env, kMissedVsyncs);

  const int scenario =
      static_cast<int>(JankScenario::COMBINED_WEBVIEW_SCROLLING);
  HistogramTester histogram_tester;
  RecordJankMetrics(
      env,
      /* java_durations_ns= */
      base::android::JavaParamRef<jlongArray>(env, java_durations),
      /* java_missed_vsyncs = */
      base::android::JavaParamRef<jintArray>(env, java_missed_vsyncs),
      /* java_reporting_interval_start_time = */ 0,
      /* java_reporting_interval_duration = */ 1000, scenario);

  // |COMBINED_WEBVIEW_SCROLLING| scenario uses 'WebviewScrolling' suffix for
  // emitting the per frame metrics.
  const std::string kDurationName =
      "Android.FrameTimelineJank.Duration.WebviewScrolling";
  const std::string kJankyName =
      "Android.FrameTimelineJank.FrameJankStatus.WebviewScrolling";

  histogram_tester.ExpectTotalCount(kDurationName, kNumFrames);
  histogram_tester.ExpectTotalCount(kJankyName, kNumFrames);
}

class JankMetricUMARecorderPerScrollTests
    : public testing::Test,
      public testing::WithParamInterface<ScrollTestCase> {};
INSTANTIATE_TEST_SUITE_P(
    JankMetricUMARecorderPerScrollTests,
    JankMetricUMARecorderPerScrollTests,
    testing::ValuesIn<ScrollTestCase>({
        {JankScenario::WEBVIEW_SCROLLING, "EmitsSmallScrollHistogramInWebview",
         10, ".Small"},
        {JankScenario::WEBVIEW_SCROLLING, "EmitsMediumScrollHistogramInWebview",
         50, ".Medium"},
        {JankScenario::WEBVIEW_SCROLLING, "EmitsLargeScrollHistogramInWebview",
         65, ".Large"},
        {JankScenario::FEED_SCROLLING, "EmitsSmallScrollHistogramInFeed", 10,
         ".Small"},
        {JankScenario::FEED_SCROLLING, "EmitsMediumScrollHistogramInFeed", 50,
         ".Medium"},
        {JankScenario::FEED_SCROLLING, "EmitsLargeScrollHistogramInFeed", 65,
         ".Large"},
    }),
    [](const testing::TestParamInfo<
        JankMetricUMARecorderPerScrollTests::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(JankMetricUMARecorderPerScrollTests, EmitsPerScrollHistograms) {
  const ScrollTestCase& params = GetParam();

  JNIEnv* env = AttachCurrentThread();
  HistogramTester histogram_tester;
  std::vector<int64_t> durations = {1000000L, 1000000L, 1000000L};
  std::vector<int> missed_vsyncs = {0, 3, 1};
  const int expected_janky_frames = 2;
  const int expected_vsyncs_max = 3;
  const int expected_vsyncs_sum = 4;

  for (int i = durations.size(); i < params.num_frames; i++) {
    durations.push_back(1000000L);
    missed_vsyncs.push_back(0);
  }

  jlongArray java_durations = GenerateJavaLongArray(env, durations);
  jintArray java_missed_vsyncs = GenerateJavaIntArray(env, missed_vsyncs);

  RecordJankMetrics(
      env, base::android::JavaParamRef<jlongArray>(env, java_durations),
      base::android::JavaParamRef<jintArray>(env, java_missed_vsyncs),
      /* java_reporting_interval_start_time = */ 0,
      /* java_reporting_interval_duration = */ 1000,
      static_cast<int>(params.scenario));

  int expected_delayed_frames_percentage =
      (100 * expected_janky_frames) / params.num_frames;
  std::string scenario_name = "";
  if (params.scenario == JankScenario::WEBVIEW_SCROLLING) {
    scenario_name = "WebviewScrolling";
  } else {
    DCHECK_EQ(params.scenario, JankScenario::FEED_SCROLLING);
    scenario_name = "FeedScrolling";
  }
  std::string delayed_frames_histogram = "Android.FrameTimelineJank." +
                                         scenario_name +
                                         ".DelayedFramesPercentage."
                                         "PerScroll";
  std::string missed_vsyncs_max_histogram = "Android.FrameTimelineJank." +
                                            scenario_name +
                                            ".MissedVsyncsMax."
                                            "PerScroll";
  std::string missed_vsyncs_sum_histogram = "Android.FrameTimelineJank." +
                                            scenario_name +
                                            ".MissedVsyncsSum."
                                            "PerScroll";
  // Should emit non-bucketed scroll histograms.
  histogram_tester.ExpectUniqueSample(delayed_frames_histogram,
                                      expected_delayed_frames_percentage, 1);
  histogram_tester.ExpectUniqueSample(missed_vsyncs_max_histogram,
                                      expected_vsyncs_max, 1);
  histogram_tester.ExpectUniqueSample(missed_vsyncs_sum_histogram,
                                      expected_vsyncs_sum, 1);

  // Should emit bucketed scroll histograms, suffixed with scroll size like
  // Small, Medium, Large.
  histogram_tester.ExpectUniqueSample(delayed_frames_histogram + params.suffix,
                                      expected_delayed_frames_percentage, 1);
  histogram_tester.ExpectUniqueSample(
      missed_vsyncs_max_histogram + params.suffix, expected_vsyncs_max, 1);
  histogram_tester.ExpectUniqueSample(
      missed_vsyncs_sum_histogram + params.suffix, expected_vsyncs_sum, 1);
}

}  // namespace base::android
