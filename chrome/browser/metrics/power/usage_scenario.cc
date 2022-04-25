// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/usage_scenario.h"

namespace {

// A trace event is emitted when CPU usage exceeds the 95th percentile.
// Canary 7 day aggregation ending on March 15th 2022 from "PerformanceMonitor
// .ResourceCoalition.CPUTime2_10sec.*"
const ScenarioParams kVideoCaptureParams = {
    .histogram_suffix = ".VideoCapture",
    .short_interval_cpu_threshold = 1.8949,
    .trace_event_title = "High CPU - Video Capture",
};

const ScenarioParams kFullscreenVideoParams = {
    .histogram_suffix = ".FullscreenVideo",
    .short_interval_cpu_threshold = 1.4513,
    .trace_event_title = "High CPU - Fullscreen Video",
};

const ScenarioParams kEmbeddedVideoNoNavigationParams = {
    .histogram_suffix = ".EmbeddedVideo_NoNavigation",
    .short_interval_cpu_threshold = 1.5436,
    .trace_event_title = "High CPU - Embedded Video No Navigation",
};

const ScenarioParams kEmbeddedVideoWithNavigationParams = {
    .histogram_suffix = ".EmbeddedVideo_WithNavigation",
    .short_interval_cpu_threshold = 1.9999,
    .trace_event_title = "High CPU - Embedded Video With Navigation",
};

const ScenarioParams kAudioParams = {
    .histogram_suffix = ".Audio",
    .short_interval_cpu_threshold = 1.5110,
    .trace_event_title = "High CPU - Audio",
};

const ScenarioParams kNavigationParams = {
    .histogram_suffix = ".Navigation",
    .short_interval_cpu_threshold = 1.9999,
    .trace_event_title = "High CPU - Navigation",
};

const ScenarioParams kInteractionParams = {
    .histogram_suffix = ".Interaction",
    .short_interval_cpu_threshold = 1.2221,
    .trace_event_title = "High CPU - Interaction",
};

const ScenarioParams kPassiveParams = {
    .histogram_suffix = ".Passive",
    .short_interval_cpu_threshold = 0.4736,
    .trace_event_title = "High CPU - Passive",
};

#if BUILDFLAG(IS_MAC)
const ScenarioParams kAllTabsHiddenNoVideoCaptureOrAudioParams = {
    .histogram_suffix = ".AllTabsHidden_NoVideoCaptureOrAudio",
    .short_interval_cpu_threshold = 0.2095,
    .trace_event_title =
        "High CPU - All Tabs Hidden, No Video Capture or Audio",
};

const ScenarioParams kAllTabsHiddenNoVideoCaptureOrAudioRecentParams = {
    .histogram_suffix = ".AllTabsHidden_NoVideoCaptureOrAudio_Recent",
    .short_interval_cpu_threshold = 0.3302,
    .trace_event_title =
        "High CPU - All Tabs Hidden, No Video Capture or Audio (Recent)",
};

const ScenarioParams kAllTabsHiddenNoAudioParams = {
    .histogram_suffix = ".AllTabsHidden_Audio",
    .short_interval_cpu_threshold = 0.7036,
    .trace_event_title = "High CPU - All Tabs Hidden, No Audio",
};

const ScenarioParams kAllTabsHiddenNoVideoCapture = {
    .histogram_suffix = ".AllTabsHidden_VideoCapture",
    .short_interval_cpu_threshold = 0.8679,
    .trace_event_title = "High CPU - All Tabs Hidden, Video Capture",
};

const ScenarioParams kAllTabsHiddenZeroWindowParams = {
    .histogram_suffix = ".ZeroWindow",
    .short_interval_cpu_threshold = 0.0500,
    .trace_event_title = "High CPU - Zero Window",
};

const ScenarioParams kAllTabsHiddenZeroWindowRecentParams = {
    .histogram_suffix = ".ZeroWindow_Recent",
    .short_interval_cpu_threshold = 0.0745,
    .trace_event_title = "High CPU - Zero Window (Recent)",
};
#endif

const ScenarioParams& GetScenarioParamsWithVisibleWindow(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  DCHECK_GT(interval_data.max_visible_window_count, 0);

  if (!interval_data.time_capturing_video.is_zero())
    return kVideoCaptureParams;
  if (!interval_data.time_playing_video_full_screen_single_monitor.is_zero())
    return kFullscreenVideoParams;
  if (!interval_data.time_playing_video_in_visible_tab.is_zero()) {
    // Note: UKM data reveals that navigations are infrequent when a video is
    // playing in fullscreen, when video is captured or when audio is playing.
    // For that reason, there is no distinct suffix for navigation vs. no
    // navigation in these cases.
    if (interval_data.top_level_navigation_count == 0)
      return kEmbeddedVideoNoNavigationParams;
    return kEmbeddedVideoWithNavigationParams;
  }
  if (!interval_data.time_playing_audio.is_zero())
    return kAudioParams;
  if (interval_data.top_level_navigation_count > 0)
    return kNavigationParams;
  if (interval_data.user_interaction_count > 0)
    return kInteractionParams;
  return kPassiveParams;
}

// Helper function for GetLongIntervalSuffixes().
const char* GetLongIntervalScenarioSuffix(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  if (interval_data.max_tab_count == 0)
    return ".ZeroWindow";
  if (interval_data.max_visible_window_count == 0) {
    if (!interval_data.time_capturing_video.is_zero())
      return ".AllTabsHidden_VideoCapture";
    if (!interval_data.time_playing_audio.is_zero())
      return ".AllTabsHidden_Audio";
    return ".AllTabsHidden_NoVideoCaptureOrAudio";
  }
  return GetScenarioParamsWithVisibleWindow(interval_data).histogram_suffix;
}

}  // namespace

// Returns suffixes to use for histograms related to a long interval described
// by `interval_data`.
std::vector<const char*> GetLongIntervalSuffixes(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // Histograms are recorded without suffix and with a scenario-specific
  // suffix.
  return {"", GetLongIntervalScenarioSuffix(interval_data)};
}

#if BUILDFLAG(IS_MAC)
const ScenarioParams& GetShortIntervalScenarioParams(
    const UsageScenarioDataStore::IntervalData& short_interval_data,
    const UsageScenarioDataStore::IntervalData& pre_interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  if (short_interval_data.max_tab_count == 0) {
    if (pre_interval_data.max_tab_count != 0)
      return kAllTabsHiddenZeroWindowRecentParams;
    return kAllTabsHiddenZeroWindowParams;
  }
  if (short_interval_data.max_visible_window_count == 0) {
    if (!short_interval_data.time_capturing_video.is_zero())
      return kAllTabsHiddenNoVideoCapture;
    if (!short_interval_data.time_playing_audio.is_zero())
      return kAllTabsHiddenNoAudioParams;
    if (pre_interval_data.max_visible_window_count != 0 ||
        !pre_interval_data.time_capturing_video.is_zero() ||
        !pre_interval_data.time_playing_audio.is_zero()) {
      return kAllTabsHiddenNoVideoCaptureOrAudioRecentParams;
    }
    return kAllTabsHiddenNoVideoCaptureOrAudioParams;
  }

  return GetScenarioParamsWithVisibleWindow(short_interval_data);
}
#endif  // BUILDFLAG(IS_MAC)
