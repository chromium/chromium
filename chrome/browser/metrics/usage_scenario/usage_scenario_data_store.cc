// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"

#include <algorithm>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

UsageScenarioDataStore::UsageScenarioDataStore() = default;

UsageScenarioDataStore::~UsageScenarioDataStore() = default;

UsageScenarioDataStore::IntervalData::IntervalData() = default;
UsageScenarioDataStore::IntervalData::IntervalData(const IntervalData&) =
    default;
UsageScenarioDataStore::IntervalData&
UsageScenarioDataStore::IntervalData::operator=(const IntervalData&) = default;

UsageScenarioDataStoreImpl::UsageScenarioDataStoreImpl()
    : UsageScenarioDataStoreImpl(base::DefaultTickClock::GetInstance()) {}
UsageScenarioDataStoreImpl::UsageScenarioDataStoreImpl(
    const base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      start_time_(tick_clock_->NowTicks()),
      last_interaction_with_browser_timestamp_(start_time_) {}

UsageScenarioDataStoreImpl::~UsageScenarioDataStoreImpl() = default;

UsageScenarioDataStoreImpl::IntervalData
UsageScenarioDataStoreImpl::ResetIntervalData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto now = tick_clock_->NowTicks();

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

  if (!capturing_video_since_.is_null()) {
    capturing_video_since_ = now;
  }

  if (!playing_audio_since_.is_null()) {
    playing_audio_since_ = now;
  }

  if (!playing_video_in_active_tab_since_.is_null()) {
    playing_video_in_active_tab_since_ = now;
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
  last_interaction_with_browser_timestamp_ = base::TimeTicks::Now();
}

void UsageScenarioDataStoreImpl::OnFullScreenVideoStartsOnSingleMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_playing_full_screen_video_single_monitor_since_.is_null());
  is_playing_full_screen_video_single_monitor_since_ = tick_clock_->NowTicks();
}

void UsageScenarioDataStoreImpl::OnFullScreenVideoEndsOnSingleMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_playing_full_screen_video_single_monitor_since_.is_null());
  interval_data_.time_playing_video_full_screen_single_monitor +=
      tick_clock_->NowTicks() -
      is_playing_full_screen_video_single_monitor_since_;
  is_playing_full_screen_video_single_monitor_since_ = base::TimeTicks();
}

void UsageScenarioDataStoreImpl::OnWebRTCConnectionOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Grab the current timestamp if there's no other WebRTC connection.
  if (webrtc_open_connection_count_ == 0) {
    DCHECK(has_opened_webrtc_connection_since_.is_null());
    has_opened_webrtc_connection_since_ = tick_clock_->NowTicks();
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
        tick_clock_->NowTicks() - has_opened_webrtc_connection_since_;
    has_opened_webrtc_connection_since_ = base::TimeTicks();
  }
}

void UsageScenarioDataStoreImpl::OnIsCapturingVideoStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents_capturing_video_ == 0) {
    DCHECK(capturing_video_since_.is_null());
    capturing_video_since_ = tick_clock_->NowTicks();
  }
  ++web_contents_capturing_video_;
}

void UsageScenarioDataStoreImpl::OnIsCapturingVideoEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(web_contents_capturing_video_, 0U);
  --web_contents_capturing_video_;

  // If this was the last tab capturing video then the interval data should be
  // updated.
  if (web_contents_capturing_video_ == 0) {
    DCHECK(!capturing_video_since_.is_null());
    interval_data_.time_capturing_video +=
        tick_clock_->NowTicks() - capturing_video_since_;
    capturing_video_since_ = base::TimeTicks();
  }
}

void UsageScenarioDataStoreImpl::OnAudioStarts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Grab the current timestamp if there's no other tabs playing audio.
  if (tabs_playing_audio_ == 0) {
    DCHECK(playing_audio_since_.is_null());
    playing_audio_since_ = tick_clock_->NowTicks();
  }
  ++tabs_playing_audio_;
  DCHECK_GE(current_tab_count_, tabs_playing_audio_);
}

void UsageScenarioDataStoreImpl::OnAudioStops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(tabs_playing_audio_, 0U);
  --tabs_playing_audio_;
  DCHECK_GE(current_tab_count_, tabs_playing_audio_);

  // If this was the last tab playing audio then the interval data should be
  // updated.
  if (tabs_playing_audio_ == 0) {
    DCHECK(!playing_audio_since_.is_null());
    interval_data_.time_playing_audio +=
        tick_clock_->NowTicks() - playing_audio_since_;
    playing_audio_since_ = base::TimeTicks();
  }
}

void UsageScenarioDataStoreImpl::OnSleepEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interval_data_.sleep_events++;
}

void UsageScenarioDataStoreImpl::OnVideoStartsInVisibleTab() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++visible_tabs_playing_video_;
  DCHECK_GE(current_visible_window_count_, visible_tabs_playing_video_);
  if (visible_tabs_playing_video_ == 1)
    playing_video_in_active_tab_since_ = tick_clock_->NowTicks();
}

void UsageScenarioDataStoreImpl::OnVideoStopsInVisibleTab() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(visible_tabs_playing_video_, 0U);
  --visible_tabs_playing_video_;

  // If this was the last visible tab playing video then the interval data
  // should be updated.
  if (visible_tabs_playing_video_ == 0) {
    DCHECK(!playing_video_in_active_tab_since_.is_null());
    interval_data_.time_playing_video_in_visible_tab +=
        tick_clock_->NowTicks() - playing_video_in_active_tab_since_;
    playing_video_in_active_tab_since_ = base::TimeTicks();
  }
}

void UsageScenarioDataStoreImpl::OnUkmSourceBecameVisible(
    const ukm::SourceId& source,
    const url::Origin& origin,
    extensions::ExtensionIdSet extensions_with_content_scripts) {
  DCHECK_NE(ukm::kInvalidSourceId, source);
  auto& origin_map_iter = origin_info_map_[origin];
  auto& source_id_iter = origin_map_iter[source];

  DCHECK(source_id_iter.visible_timestamp.is_null());
  source_id_iter.visible_timestamp = tick_clock_->NowTicks();

  extensions_with_content_scripts_.merge(extensions_with_content_scripts);
}

void UsageScenarioDataStoreImpl::OnUkmSourceBecameHidden(
    const ukm::SourceId& source,
    const url::Origin& origin) {
  DCHECK_NE(ukm::kInvalidSourceId, source);
  auto& origin_map_iter = origin_info_map_[origin];
  auto& source_id_iter = origin_map_iter[source];

  DCHECK(!source_id_iter.visible_timestamp.is_null());
  source_id_iter.cumulative_visible_time +=
      tick_clock_->NowTicks() - source_id_iter.visible_timestamp;
  source_id_iter.visible_timestamp = base::TimeTicks();
}

base::flat_set<ukm::SourceId>
UsageScenarioDataStoreImpl::GetVisibleSourceIdsForTesting() {
  base::flat_set<ukm::SourceId> ret;

  for (auto& origin_iter : origin_info_map_) {
    for (auto& source_ids_for_origin_iter : origin_iter.second) {
      if (!source_ids_for_origin_iter.second.visible_timestamp.is_null()) {
        ret.insert(source_ids_for_origin_iter.first);
      }
    }
  }
  return ret;
}

bool UsageScenarioDataStoreImpl::TrackingPlayingVideoInActiveTabForTesting()
    const {
  return !playing_video_in_active_tab_since_.is_null();
}

bool UsageScenarioDataStoreImpl::
    TrackingPlayingFullScreenVideoSingleMonitorForTesting() const {
  return !is_playing_full_screen_video_single_monitor_since_.is_null();
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

  if (!capturing_video_since_.is_null()) {
    interval_data_.time_capturing_video += now - capturing_video_since_;
  }

  if (!playing_audio_since_.is_null()) {
    interval_data_.time_playing_audio += now - playing_audio_since_;
  }

  if (!playing_video_in_active_tab_since_.is_null()) {
    interval_data_.time_playing_video_in_visible_tab +=
        now - playing_video_in_active_tab_since_;
  }

  interval_data_.time_since_last_user_interaction_with_browser =
      now - last_interaction_with_browser_timestamp_;

  interval_data_.num_extensions_with_content_scripts =
      extensions_with_content_scripts_.size();
  extensions_with_content_scripts_.clear();

  base::TimeDelta origin_visible_for_longest_time_duration;
  // Finalize the interval data and find the origin that has been visible for
  // the longest period of time.
  for (auto origin_iter = origin_info_map_.begin();
       origin_iter != origin_info_map_.end();) {
    // Compute the total visible time for this origin. This can exceed the
    // interval length if multiple tabs with the same origin are visible at the
    // same time.
    base::TimeDelta origin_visible_duration;
    base::TimeDelta longest_visible_sourceid_duration;
    ukm::SourceId longest_visible_sourceid = ukm::kInvalidSourceId;
    for (auto iter = origin_iter->second.begin();
         iter != origin_iter->second.end();) {
      // If this SourceID is still visible then its cumulative time has to be
      // updated.
      if (!iter->second.visible_timestamp.is_null()) {
        DCHECK(!iter->second.visible_timestamp.is_null());
        iter->second.cumulative_visible_time +=
            now - iter->second.visible_timestamp;
        iter->second.visible_timestamp = now;
      }

      // Track the SourceID that has been visible the longest.
      if (iter->second.cumulative_visible_time >
          longest_visible_sourceid_duration) {
        longest_visible_sourceid = iter->first;
        longest_visible_sourceid_duration =
            iter->second.cumulative_visible_time;
      }

      origin_visible_duration += iter->second.cumulative_visible_time;

      // Remove the non visible source IDs from the map, they're not needed
      // anymore
      if (!iter->second.visible_timestamp.is_null()) {
        // Reset the cumulative timestamp counter as the data has been consumed.
        iter->second.cumulative_visible_time = base::TimeDelta();
        ++iter;
      } else {
        iter = origin_iter->second.erase(iter);
      }
    }

    bool update_interval_data =
        origin_visible_duration > origin_visible_for_longest_time_duration;
    // In case of equality check if there's one sourceID that has been visible
    // for a longer time than the currently recorded one.
    if (origin_visible_duration == origin_visible_for_longest_time_duration) {
      if (longest_visible_sourceid_duration >
          interval_data_.source_id_for_longest_visible_origin_duration) {
        update_interval_data = true;
      }
    }

    if (update_interval_data) {
      origin_visible_for_longest_time_duration = origin_visible_duration;
      interval_data_.longest_visible_origin_duration = origin_visible_duration;
      interval_data_.source_id_for_longest_visible_origin_duration =
          longest_visible_sourceid_duration;
      interval_data_.source_id_for_longest_visible_origin =
          longest_visible_sourceid;
    }

    // Remove the origins that contain no visible SourceIDs.
    if (origin_iter->second.empty()) {
      origin_iter = origin_info_map_.erase(origin_iter);
    } else {
      ++origin_iter;
    }
  }
}
