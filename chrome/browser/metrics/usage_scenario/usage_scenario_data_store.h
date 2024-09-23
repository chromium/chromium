// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_DATA_STORE_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_DATA_STORE_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace metrics {
class TabUsageScenarioTrackerBrowserTest;
}

// Stores the data necessary to analyze the usage pattern during a given
// interval of time. There are 2 types of data tracked by this class:
//   - Current state data: e.g. the current uptime.
//   - Interval data: e.g. whether or not there's been a user interaction since
//     the last call to ResetIntervalData.
//
// By default this class assumes that no tabs exists when it's created. If this
// isn't true then the data providers need to make the appropriate calls to set
// the correct initial state.
//
// The interval's length needs to be enforced by the owner of this class, it
// should call ResetIntervalData regularly to get the usage data and reset it.
class UsageScenarioDataStore {
 public:
  UsageScenarioDataStore();
  UsageScenarioDataStore(const UsageScenarioDataStore& rhs) = delete;
  UsageScenarioDataStore& operator=(const UsageScenarioDataStore& rhs) = delete;
  virtual ~UsageScenarioDataStore() = 0;

  // Used to store data between 2 calls to ResetIntervalData.
  struct IntervalData {
    IntervalData();
    IntervalData(const IntervalData&);
    IntervalData& operator=(const IntervalData&);

    // The uptime at the end of the interval.
    base::TimeDelta uptime_at_interval_end;
    // The maximum number of tabs that existed at the same time.
    uint16_t max_tab_count = 0;
    // The maximum number of windows that have been visible at the same time.
    uint16_t max_visible_window_count = 0;
    // Number of main frame different-document navigations in tabs.
    uint16_t top_level_navigation_count = 0;
    // The number of tabs that have been closed.
    uint16_t tabs_closed_during_interval = 0;
    // Number of user interaction (scroll, click or typing).
    uint16_t user_interaction_count = 0;
    // The time spent playing video full screen in a single-monitor situation.
    base::TimeDelta time_playing_video_full_screen_single_monitor;
    // The time spent with at least one opened WebRTC connection.
    base::TimeDelta time_with_open_webrtc_connection;
    // The time spent with at least one WebContents capturing video.
    base::TimeDelta time_capturing_video;
    // The time spent playing video in at least one visible tab.
    base::TimeDelta time_playing_video_in_visible_tab;
    // The time spent playing audio in at least one tab.
    base::TimeDelta time_playing_audio;
    // The time since the last user interaction with the browser at the end of
    // the interval. This time can exceed the length of the interval.
    base::TimeDelta time_since_last_user_interaction_with_browser;

    // The SourceID that has been visible for the longest period of time for the
    // origin that has been visible for the longest period of time during the
    // interval. E.g.:
    //   - SourceID 1 and 2 are for the same origin and are visible respectively
    //     for 2 and 3 seconds each during the interval.
    //   - SourceID 3 is for a different origin and is visible for 4 second.
    //   - This will report Source ID 2 with a duration of 3 seconds.
    //
    // In case of equality for an interval a SourceID will be randomly picked.
    // In case of equality between origins this will report the data for the
    // origin that contains the sourceID that has been visible the longest (or
    // a random one in case of equality).
    ukm::SourceId source_id_for_longest_visible_origin = ukm::kInvalidSourceId;

    // The visibility time for |source_id_for_longest_visible_origin|.
    base::TimeDelta source_id_for_longest_visible_origin_duration;

    // The visibility time for the Origin associated with
    // |source_id_for_longest_visible_origin|. This could be greater than
    // |source_id_for_longest_visible_origin_duration| if there's multiple tabs
    // for the longest visible origin visible during the interval.
    base::TimeDelta longest_visible_origin_duration;

    // The number of times the system has been put to sleep during the interval.
    uint8_t sleep_events = 0;

    // The number of extensions that ran content scripts during the interval.
    size_t num_extensions_with_content_scripts = 0;
  };

  // Reset the interval data with the current state information and returns the
  // data for the past interval (since the last call to ResetIntervalData or the
  // creation of this object if this is the first call).
  virtual IntervalData ResetIntervalData() = 0;
};

// Concrete implementation of a UsageScenarioDataStore that expose the functions
// allowing to update its internal state.
//
// This class isn't thread safe and all functions should be called from a single
// sequence. This is enforced via a sequence checker.
class UsageScenarioDataStoreImpl : public UsageScenarioDataStore {
 public:
  UsageScenarioDataStoreImpl();
  UsageScenarioDataStoreImpl(const UsageScenarioDataStoreImpl& rhs) = delete;
  UsageScenarioDataStoreImpl& operator=(const UsageScenarioDataStoreImpl& rhs) =
      delete;
  ~UsageScenarioDataStoreImpl() override;

  IntervalData ResetIntervalData() override;

  // Set of functions used to maintain the current state, these should only be
  // called by a UsageScenarioDataInfoProvider instance. It is important to log
  // all events to ensure the integrity of the data store, e.g. if a tab
  // currently using WebRTC is closed the 2 following functions should be
  // called:
  //   - OnTabStopUsingWebRTC()
  //   - OnTabClosed()

  void OnTabAdded();
  void OnTabClosed();
  void OnWindowVisible();
  void OnWindowHidden();
  void OnTopLevelNavigation();
  void OnUserInteraction();
  void OnFullScreenVideoStartsOnSingleMonitor();
  void OnFullScreenVideoEndsOnSingleMonitor();
  void OnWebRTCConnectionOpened();
  void OnWebRTCConnectionClosed();
  void OnIsCapturingVideoStarted();
  void OnIsCapturingVideoEnded();
  void OnAudioStarts();
  void OnAudioStops();
  void OnSleepEvent();

  // Should be called when a video starts in a visible tab or when a non visible
  // tab playing video becomes visible.
  void OnVideoStartsInVisibleTab();

  // Should be called when a video stops in a visible tab or when a visible
  // tab playing video becomes non visible.
  void OnVideoStopsInVisibleTab();

  void OnUkmSourceBecameVisible(
      const ukm::SourceId& source,
      const url::Origin& origin,
      extensions::ExtensionIdSet extensions_with_content_scripts);
  void OnUkmSourceBecameHidden(const ukm::SourceId& source,
                               const url::Origin& origin);

  const IntervalData& GetIntervalDataForTesting() { return interval_data_; }

  uint16_t current_tab_count_for_testing() { return current_tab_count_; }
  uint16_t current_visible_window_count_for_testing() {
    return current_visible_window_count_;
  }

  base::flat_set<ukm::SourceId> GetVisibleSourceIdsForTesting();

  bool TrackingPlayingVideoInActiveTabForTesting() const;
  bool TrackingPlayingFullScreenVideoSingleMonitorForTesting() const;

 private:
  friend class metrics::TabUsageScenarioTrackerBrowserTest;

  explicit UsageScenarioDataStoreImpl(const base::TickClock* tick_clock);

  // Information about a ukm::SourceId that has been visible during an interval
  // of time.
  struct SourceIdData {
    // The timestamp when the SourceID became visible, null if the sourceID
    // isn't visible.
    base::TimeTicks visible_timestamp;
    // The total visible time during the interval.
    base::TimeDelta cumulative_visible_time;
  };
  using OriginData = base::flat_map<ukm::SourceId, SourceIdData>;
  using OriginInfoMap = base::flat_map<url::Origin, OriginData>;

  // Finalize the interval data based on the data contained in
  // |interval_details_| and |origin_info_map_| and remove the SourceIdData that
  // don't need to be tracked anymore.
  void FinalizeIntervalData(base::TimeTicks now);

  // The clock used by this class.
  raw_ptr<const base::TickClock> tick_clock_;

  // The current tab count.
  uint16_t current_tab_count_ = 0;

  // The current number of visible windows.
  uint16_t current_visible_window_count_ = 0;

  // The timestamp of the beginning of a full screen video session when
  // there's only one monitor available. Reset to |now| when an interval ends
  // (when ResetIntervalData is called).
  base::TimeTicks is_playing_full_screen_video_single_monitor_since_;

  // The number of opened WebRTC connections.
  uint16_t webrtc_open_connection_count_ = 0;

  // The timestamp of the beginning of the WebRTC session that has caused
  // |webrtc_connection_count| to increase to 1. Reset to |now| when an interval
  // ends (when ResetIntervalData is called).
  base::TimeTicks has_opened_webrtc_connection_since_;

  // The number of WebContents capturing video (e.g. webcam). Usually a tab, but
  // some exceptions exist (e.g. OOBE WebUI on ChromeOS).
  uint16_t web_contents_capturing_video_ = 0;

  // The timestamp of the beginning of a video capture session that has caused
  // |web_contents_capturing_video_| to increase to 1. Reset to |now| when an
  // internal ends (when ResetIntervalData is called).
  base::TimeTicks capturing_video_since_;

  // The number of tabs playing audio.
  uint16_t tabs_playing_audio_ = 0;

  // The timestamp of the beginning of an audio session that has caused
  // |tabs_playing_audio_| to increase to 1. Reset to |now| when an interval
  // ends (when ResetIntervalData is called).
  base::TimeTicks playing_audio_since_;

  // The number of visible tabs playing at least one video.
  uint16_t visible_tabs_playing_video_ = 0;

  // Timestamp grabbed when |visible_tabs_playing_video_| increase to 1. Reset
  // to |now| when an interval ends (when ResetIntervalData is called).
  base::TimeTicks playing_video_in_active_tab_since_;

  // The application start time.
  const base::TimeTicks start_time_;

  // The timestamp of the most recent call to OnUserInteraction(), equal to
  // |start_time_| if this hasn't been called yet.
  base::TimeTicks last_interaction_with_browser_timestamp_;

  // Information about the origins that have been visible during the interval.
  OriginInfoMap origin_info_map_;

  // Extensions that ran content scripts on visible origins during the interval.
  extensions::ExtensionIdSet extensions_with_content_scripts_;

  IntervalData interval_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_DATA_STORE_H_
