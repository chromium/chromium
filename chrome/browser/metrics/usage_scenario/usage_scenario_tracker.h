// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_TRACKER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_TRACKER_H_

#include "base/sequence_checker.h"
#include "chrome/browser/metrics/usage_scenario/system_event_provider.h"
#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/video_capture_event_provider.h"

// Registers as an observer to various components to maintain a
// UsageScenarioDataStore.
class UsageScenarioTracker {
 public:
  UsageScenarioTracker();
  UsageScenarioTracker(const UsageScenarioTracker& rhs) = delete;
  UsageScenarioTracker& operator=(const UsageScenarioTracker& rhs) = delete;
  ~UsageScenarioTracker();

  // Return the data store owned by this tracker.
  UsageScenarioDataStore* data_store() { return &data_store_; }

 private:
  UsageScenarioDataStoreImpl data_store_;

  // Track tab related information.
  metrics::TabUsageScenarioTracker tab_usage_scenario_tracker_;

  // Tracks tabs capturing video.
  VideoCaptureEventProvider video_capture_event_provider_;

  // Tracks system events.
  SystemEventProvider system_event_provider_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_USAGE_SCENARIO_TRACKER_H_
