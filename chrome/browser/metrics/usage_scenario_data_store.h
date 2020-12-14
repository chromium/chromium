// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_DATA_STORE_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_DATA_STORE_H_

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"

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

  const IntervalData& GetIntervalDataForTesting() { return interval_data_; }

 private:
  // Finalize the interval data based on the data contained in
  // |interval_details_|.
  void FinalizeIntervalData(base::TimeTicks now);

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

  // The application start time.
  const base::TimeTicks start_time_;

  IntervalData interval_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_DATA_STORE_H_
