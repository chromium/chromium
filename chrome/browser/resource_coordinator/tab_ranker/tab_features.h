// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_H_

#include <stdint.h>
#include <string>

#include "base/optional.h"

namespace assist_ranker {
class RankerExample;
}

namespace ukm {
namespace builders {
class TabManager_TabMetrics;
}
}  // namespace ukm

namespace tab_ranker {

// Tab features used for logging a Tab Ranker example to UKM or calculating a
// Tab Ranker score.
struct TabFeatures {
  TabFeatures();
  ~TabFeatures();

  TabFeatures(const TabFeatures& other);

  // Keep properties in alphabetical order to match the order in
  // TabMetricsLogger::LogBackgroundTab() and make it easier to check which
  // properties are sent via UKM.
  bool has_before_unload_handler = false;
  bool has_form_entry = false;
  bool is_pinned = false;
  int32_t key_event_count = 0;
  int32_t mouse_event_count = 0;
  int32_t mru_index = 0;
  int32_t navigation_entry_count = 0;
  // Number of times the tab has been reactivated while showing the current
  // page. Reset to 0 when a tab navigates.
  int32_t num_reactivations = 0;
  // Null if the value is not one of the core values logged to UKM.
  base::Optional<int32_t> page_transition_core_type;
  bool page_transition_from_address_bar = false;
  bool page_transition_is_redirect = false;
  // Null if the SiteEngagementService is disabled.
  base::Optional<int32_t> site_engagement_score;
  // Time since tab was backgrounded, in milliseconds.
  int32_t time_from_backgrounded = 0;
  int32_t total_tab_count = 0;
  int32_t touch_event_count = 0;
  bool was_recently_audible = false;
  bool window_is_active = false;
  int32_t window_show_state = 0;
  int32_t window_tab_count = 0;
  int32_t window_type = 0;

  // Used only for inference.
  int32_t discard_count = 0;
  std::string host;
  float frecency_score = 0.0f;
};

// Populates |tab| features to ranker example for inference.
void PopulateTabFeaturesToRankerExample(const TabFeatures& tab,
                                        assist_ranker::RankerExample* example);

// Populates |tab| features to ukm |entry| for logging.
void PopulateTabFeaturesToUkmEntry(const TabFeatures& tab,
                                   ukm::builders::TabManager_TabMetrics* entry);

}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_H_
