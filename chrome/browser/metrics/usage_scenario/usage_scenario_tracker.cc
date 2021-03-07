// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

UsageScenarioTracker::UsageScenarioTracker()
    : tab_usage_scenario_tracker_(&data_store_),
      video_capture_event_provider_(&data_store_) {
  metrics::TabStatsTracker::GetInstance()->AddObserverAndSetInitialState(
      &tab_usage_scenario_tracker_);
}

UsageScenarioTracker::~UsageScenarioTracker() = default;
