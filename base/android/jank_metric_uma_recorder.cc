// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jank_metric_uma_recorder.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "jank_metric_uma_recorder.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/jank_tracker_jni/JankMetricUMARecorder_jni.h"

namespace base::android {

namespace {

// Histogram min, max and no. of buckets.
constexpr int kVsyncCountsMin = 1;
constexpr int kVsyncCountsMax = 50;
constexpr int kVsyncCountsBuckets = 25;

enum class PerScrollHistogramType {
  kPercentage = 0,
  kMax = 1,
  kSum = 2,
};

const char* GetPerScrollHistogramName(JankScenario scenario,
                                      int num_frames,
                                      PerScrollHistogramType type,
                                      bool with_scroll_size_suffix) {
#define HISTOGRAM_NAME(hist_scenario, hist_type, length)     \
  "Android.FrameTimelineJank." #hist_scenario "." #hist_type \
  "."                                                        \
  "PerScroll" #length
  if (scenario == JankScenario::WEBVIEW_SCROLLING) {
    if (type == PerScrollHistogramType::kPercentage) {
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(WebviewScrolling, DelayedFramesPercentage,
                              /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(WebviewScrolling, DelayedFramesPercentage,
                              .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(WebviewScrolling, DelayedFramesPercentage,
                              .Medium);
      } else {
        return HISTOGRAM_NAME(WebviewScrolling, DelayedFramesPercentage,
                              .Large);
      }
    } else if (type == PerScrollHistogramType::kMax) {
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsMax, /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsMax, .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsMax, .Medium);
      } else {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsMax, .Large);
      }
    } else {
      DCHECK_EQ(type, PerScrollHistogramType::kSum);
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsSum, /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsSum, .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsSum, .Medium);
      } else {
        return HISTOGRAM_NAME(WebviewScrolling, MissedVsyncsSum, .Large);
      }
    }
  } else {
    DCHECK_EQ(scenario, JankScenario::FEED_SCROLLING);
    if (type == PerScrollHistogramType::kPercentage) {
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(FeedScrolling, DelayedFramesPercentage,
                              /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(FeedScrolling, DelayedFramesPercentage, .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(FeedScrolling, DelayedFramesPercentage, .Medium);
      } else {
        return HISTOGRAM_NAME(FeedScrolling, DelayedFramesPercentage, .Large);
      }
    } else if (type == PerScrollHistogramType::kMax) {
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsMax, /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsMax, .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsMax, .Medium);
      } else {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsMax, .Large);
      }
    } else {
      DCHECK_EQ(type, PerScrollHistogramType::kSum);
      if (!with_scroll_size_suffix) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsSum, /*no suffix*/);
      }
      if (num_frames <= 16) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsSum, .Small);
      } else if (num_frames <= 64) {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsSum, .Medium);
      } else {
        return HISTOGRAM_NAME(FeedScrolling, MissedVsyncsSum, .Large);
      }
    }
  }
#undef HISTOGRAM_NAME
}

// Emits trace event for all scenarios and per scroll histograms for webview and
// feed scrolling scenarios.
void EmitMetrics(JankScenario scenario,
                 int janky_frame_count,
                 int missed_vsyncs_max,
                 int missed_vsyncs_sum,
                 int num_presented_frames,
                 int64_t reporting_interval_start_time,
                 int64_t reporting_interval_duration) {
  DCHECK_GT(num_presented_frames, 0);
  int delayed_frames_percentage =
      (100 * janky_frame_count) / num_presented_frames;
  if (reporting_interval_start_time > 0) {
    // The following code does nothing if base tracing is disabled.
    [[maybe_unused]] int non_janky_frame_count =
        num_presented_frames - janky_frame_count;
    [[maybe_unused]] auto t = perfetto::Track(static_cast<uint64_t>(
        reporting_interval_start_time + static_cast<int>(scenario)));
    TRACE_EVENT_BEGIN(
        "android_webview.timeline,android.ui.jank",
        "JankMetricsReportingInterval", t,
        base::TimeTicks::FromUptimeMillis(reporting_interval_start_time),
        "janky_frames", janky_frame_count, "non_janky_frames",
        non_janky_frame_count, "scenario", static_cast<int>(scenario),
        "delayed_frames_percentage", delayed_frames_percentage,
        "missed_vsyns_max", missed_vsyncs_max, "missed_vsyncs_sum",
        missed_vsyncs_sum);
    TRACE_EVENT_END(
        "android_webview.timeline,android.ui.jank", t,
        base::TimeTicks::FromUptimeMillis(
            (reporting_interval_start_time + reporting_interval_duration)));
  }

  if (scenario != JankScenario::WEBVIEW_SCROLLING &&
      scenario != JankScenario::FEED_SCROLLING) {
    return;
  }
  // Emit non-bucketed per scroll metrics.
  base::UmaHistogramPercentage(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kPercentage,
                                /*with_scroll_size_suffix=*/false),
      delayed_frames_percentage);
  base::UmaHistogramCustomCounts(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kMax,
                                /*with_scroll_size_suffix=*/false),
      missed_vsyncs_max, kVsyncCountsMin, kVsyncCountsMax, kVsyncCountsBuckets);
  base::UmaHistogramCustomCounts(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kSum,
                                /*with_scroll_size_suffix=*/false),
      missed_vsyncs_sum, kVsyncCountsMin, kVsyncCountsMax, kVsyncCountsBuckets);

  // Emit bucketed per scroll metrics where scrolls are divided into three
  // buckets Small, Medium, Large.
  base::UmaHistogramPercentage(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kPercentage,
                                /*with_scroll_size_suffix=*/true),
      delayed_frames_percentage);
  base::UmaHistogramCustomCounts(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kMax,
                                /*with_scroll_size_suffix=*/true),
      missed_vsyncs_max, kVsyncCountsMin, kVsyncCountsMax, kVsyncCountsBuckets);
  base::UmaHistogramCustomCounts(
      GetPerScrollHistogramName(scenario, num_presented_frames,
                                PerScrollHistogramType::kSum,
                                /*with_scroll_size_suffix=*/true),
      missed_vsyncs_sum, kVsyncCountsMin, kVsyncCountsMax, kVsyncCountsBuckets);
}

}  // namespace

const char* GetAndroidFrameTimelineJankHistogramName(JankScenario scenario) {
#define HISTOGRAM_NAME(x) "Android.FrameTimelineJank.FrameJankStatus." #x
  switch (scenario) {
    case JankScenario::PERIODIC_REPORTING:
      return HISTOGRAM_NAME(Total);
    case JankScenario::OMNIBOX_FOCUS:
      return HISTOGRAM_NAME(OmniboxFocus);
    case JankScenario::NEW_TAB_PAGE:
      return HISTOGRAM_NAME(NewTabPage);
    case JankScenario::STARTUP:
      return HISTOGRAM_NAME(Startup);
    case JankScenario::TAB_SWITCHER:
      return HISTOGRAM_NAME(TabSwitcher);
    case JankScenario::OPEN_LINK_IN_NEW_TAB:
      return HISTOGRAM_NAME(OpenLinkInNewTab);
    case JankScenario::START_SURFACE_HOMEPAGE:
      return HISTOGRAM_NAME(StartSurfaceHomepage);
    case JankScenario::START_SURFACE_TAB_SWITCHER:
      return HISTOGRAM_NAME(StartSurfaceTabSwitcher);
    case JankScenario::FEED_SCROLLING:
      return HISTOGRAM_NAME(FeedScrolling);
    case JankScenario::WEBVIEW_SCROLLING:
      return HISTOGRAM_NAME(WebviewScrolling);
    case JankScenario::COMBINED_WEBVIEW_SCROLLING:
      // Emit per frame metrics for combined scrolling scenario with same
      // histogram name as webview scrolling. This is fine since we don't emit
      // per frame metrics for |WEBVIEW_SCROLLING| scenario.
      return HISTOGRAM_NAME(WebviewScrolling);
    default:
      NOTREACHED();
  }
#undef HISTOGRAM_NAME
}

const char* GetAndroidFrameTimelineDurationHistogramName(
    JankScenario scenario) {
#define HISTOGRAM_NAME(x) "Android.FrameTimelineJank.Duration." #x
  switch (scenario) {
    case JankScenario::PERIODIC_REPORTING:
      return HISTOGRAM_NAME(Total);
    case JankScenario::OMNIBOX_FOCUS:
      return HISTOGRAM_NAME(OmniboxFocus);
    case JankScenario::NEW_TAB_PAGE:
      return HISTOGRAM_NAME(NewTabPage);
    case JankScenario::STARTUP:
      return HISTOGRAM_NAME(Startup);
    case JankScenario::TAB_SWITCHER:
      return HISTOGRAM_NAME(TabSwitcher);
    case JankScenario::OPEN_LINK_IN_NEW_TAB:
      return HISTOGRAM_NAME(OpenLinkInNewTab);
    case JankScenario::START_SURFACE_HOMEPAGE:
      return HISTOGRAM_NAME(StartSurfaceHomepage);
    case JankScenario::START_SURFACE_TAB_SWITCHER:
      return HISTOGRAM_NAME(StartSurfaceTabSwitcher);
    case JankScenario::FEED_SCROLLING:
      return HISTOGRAM_NAME(FeedScrolling);
    case JankScenario::WEBVIEW_SCROLLING:
      return HISTOGRAM_NAME(WebviewScrolling);
    case JankScenario::COMBINED_WEBVIEW_SCROLLING:
      // Emit per frame metrics for combined scrolling scenario with same
      // histogram name as webview scrolling. This is fine since we don't emit
      // per frame metrics for |WEBVIEW_SCROLLING| scenario.
      return HISTOGRAM_NAME(WebviewScrolling);
    default:
      NOTREACHED();
  }
#undef HISTOGRAM_NAME
}

// This function is called from Java with JNI, it's declared in
// base/base_jni/JankMetricUMARecorder_jni.h which is an autogenerated
// header. The actual implementation is in RecordJankMetrics for simpler
// testing.
void JNI_JankMetricUMARecorder_RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jintArray>& java_missed_vsyncs,
    jlong java_reporting_interval_start_time,
    jlong java_reporting_interval_duration,
    jint java_scenario_enum) {
  RecordJankMetrics(env, java_durations_ns, java_missed_vsyncs,
                    java_reporting_interval_start_time,
                    java_reporting_interval_duration, java_scenario_enum);
}

void RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jintArray>& java_missed_vsyncs,
    jlong java_reporting_interval_start_time,
    jlong java_reporting_interval_duration,
    jint java_scenario_enum) {
  std::vector<int64_t> durations_ns;
  JavaLongArrayToInt64Vector(env, java_durations_ns, &durations_ns);

  std::vector<int> missed_vsyncs;
  JavaIntArrayToIntVector(env, java_missed_vsyncs, &missed_vsyncs);

  JankScenario scenario = static_cast<JankScenario>(java_scenario_enum);

  const char* frame_duration_histogram_name =
      GetAndroidFrameTimelineDurationHistogramName(scenario);
  const char* janky_frames_per_scenario_histogram_name =
      GetAndroidFrameTimelineJankHistogramName(scenario);

  // We don't want to emit per frame metircs for WEBVIEW SCROLLING scenario
  // which tracks individual scrolls differentiated by gesture_scroll_id.
  // Scroll related per frame metrics are emitted from
  // COMBINED_WEBVIEW_SCROLLING scenario to avoid emitting duplicate metrics for
  // overlapping scrolls.
  const bool emit_per_frame_metrics =
      scenario != JankScenario::WEBVIEW_SCROLLING;

  if (emit_per_frame_metrics) {
    for (const int64_t frame_duration_ns : durations_ns) {
      base::UmaHistogramTimes(frame_duration_histogram_name,
                              base::Nanoseconds(frame_duration_ns));
    }
  }

  int janky_frame_count = 0;
  int missed_vsyncs_max = 0;
  int missed_vsyncs_sum = 0;
  const int num_presented_frames = static_cast<int>(missed_vsyncs.size());

  for (int curr_frame_missed_vsyncs : missed_vsyncs) {
    bool is_janky = curr_frame_missed_vsyncs > 0;
    if (curr_frame_missed_vsyncs > missed_vsyncs_max) {
      missed_vsyncs_max = curr_frame_missed_vsyncs;
    }
    missed_vsyncs_sum += curr_frame_missed_vsyncs;

    if (emit_per_frame_metrics) {
      base::UmaHistogramEnumeration(
          janky_frames_per_scenario_histogram_name,
          is_janky ? FrameJankStatus::kJanky : FrameJankStatus::kNonJanky);
    }
    if (is_janky) {
      ++janky_frame_count;
    }
  }

  if (num_presented_frames > 0) {
    EmitMetrics(scenario, janky_frame_count, missed_vsyncs_max,
                missed_vsyncs_sum, num_presented_frames,
                java_reporting_interval_start_time,
                java_reporting_interval_duration);
  }
}

}  // namespace base::android
