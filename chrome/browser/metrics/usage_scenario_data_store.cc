// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario_data_store.h"

#include <algorithm>

#include "base/time/time.h"

UsageScenarioDataStore::UsageScenarioDataStore() = default;

UsageScenarioDataStore::~UsageScenarioDataStore() = default;

UsageScenarioDataStore::IntervalData::IntervalData() = default;

UsageScenarioDataStoreImpl::UsageScenarioDataStoreImpl()
    : start_time_(base::TimeTicks::Now()) {}

UsageScenarioDataStoreImpl::~UsageScenarioDataStoreImpl() = default;

UsageScenarioDataStoreImpl::IntervalData
UsageScenarioDataStoreImpl::ResetIntervalData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto now = base::TimeTicks::Now();

  FinalizeIntervalData(now);
  IntervalData ret = interval_data_;

  ret.uptime_at_interval_end = now - start_time_;

  // Start by resetting the interval data entirely.
  interval_data_ = {};

  // Set the |interval_data_| fields that are based on the current state.

  // The maximum number of tabs and visible windows for the next interval is now
  // equal to the current counts for these metrics.
  interval_data_.max_tab_count = current_tab_count_;
  interval_data_.max_visible_window_count = current_visible_window_count_;

  if (!is_playing_full_screen_video_single_monitor_since_.is_null()) {
    is_playing_full_screen_video_single_monitor_since_ = now;
  }

  if (!has_opened_webrtc_connection_since_.is_null()) {
    has_opened_webrtc_connection_since_ = now;
  }

  return ret;
}

void UsageScenarioDataStoreImpl::OnTabAdded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++current_tab_count_;
  interval_data_.max_tab_count =
      std::max(interval_data_.max_tab_count, current_tab_count_);
}

void UsageScenarioDataStoreImpl::OnTabClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(current_tab_count_, 0U);
  --current_tab_count_;
  DCHECK_GE(current_tab_count_, current_visible_window_count_);
  ++interval_data_.tabs_closed_during_interval;
}

void UsageScenarioDataStoreImpl::OnWindowVisible() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++current_visible_window_count_;
  DCHECK_GE(current_tab_count_, current_visible_window_count_);
  interval_data_.max_visible_window_count = std::max(
      interval_data_.max_visible_window_count, current_visible_window_count_);
}

void UsageScenarioDataStoreImpl::OnWindowHidden() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(current_visible_window_count_, 0U);
  --current_visible_window_count_;
}

void UsageScenarioDataStoreImpl::OnTopLevelNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++interval_data_.top_level_navigation_count;
}

void UsageScenarioDataStoreImpl::OnUserInteraction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++interval_data_.user_interaction_count;
}

void UsageScenarioDataStoreImpl::OnFullScreenVideoStartsOnSingleMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_playing_full_screen_video_single_monitor_since_.is_null());
  is_playing_full_screen_video_single_monitor_since_ = base::TimeTicks::Now();
}

void UsageScenarioDataStoreImpl::OnFullScreenVideoEndsOnSingleMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_playing_full_screen_video_single_monitor_since_.is_null());
  interval_data_.time_playing_video_full_screen_single_monitor +=
      base::TimeTicks::Now() -
      is_playing_full_screen_video_single_monitor_since_;
  is_playing_full_screen_video_single_monitor_since_ = base::TimeTicks();
}

void UsageScenarioDataStoreImpl::OnWebRTCConnectionOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Grab the current timestamp if there's no remaining WebRTC connection.
  if (webrtc_open_connection_count_ == 0) {
    DCHECK(has_opened_webrtc_connection_since_.is_null());
    has_opened_webrtc_connection_since_ = base::TimeTicks::Now();
  }
  ++webrtc_open_connection_count_;
  DCHECK_GE(current_tab_count_, webrtc_open_connection_count_);
}

void UsageScenarioDataStoreImpl::OnWebRTCConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(webrtc_open_connection_count_, 0U);
  --webrtc_open_connection_count_;
  DCHECK_GE(current_tab_count_, webrtc_open_connection_count_);

  // If this was the last tab using WebRTC then the interval data should be
  // updated.
  if (webrtc_open_connection_count_ == 0) {
    DCHECK(!has_opened_webrtc_connection_since_.is_null());
    interval_data_.time_with_open_webrtc_connection +=
        base::TimeTicks::Now() - has_opened_webrtc_connection_since_;
    has_opened_webrtc_connection_since_ = base::TimeTicks();
  }
}

void UsageScenarioDataStoreImpl::FinalizeIntervalData(base::TimeTicks now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update feature usage durations in |interval_data_|.

  if (!is_playing_full_screen_video_single_monitor_since_.is_null()) {
    interval_data_.time_playing_video_full_screen_single_monitor +=
        now - is_playing_full_screen_video_single_monitor_since_;
  }

  if (!has_opened_webrtc_connection_since_.is_null()) {
    interval_data_.time_with_open_webrtc_connection +=
        now - has_opened_webrtc_connection_since_;
  }
}
