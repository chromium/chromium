// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_features_test_helper.h"

#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"

namespace tab_ranker {

// The following two functions are used in multiple tests to make sure the
// conversion, logging and inferencing use the same group of features.
// Returns a default tab features with some field unset.
TabFeatures GetPartialTabFeaturesForTesting() {
  TabFeatures tab;

  tab.has_before_unload_handler = true;
  tab.has_form_entry = true;
  tab.is_pinned = true;
  tab.key_event_count = 121;
  tab.mouse_event_count = 122;
  tab.mru_index = 13;
  tab.navigation_entry_count = 124;
  tab.num_reactivations = 125;
  tab.page_transition_from_address_bar = true;
  tab.page_transition_is_redirect = true;
  tab.time_from_backgrounded = 110000;
  tab.total_tab_count = 130;
  tab.touch_event_count = 128;
  tab.was_recently_audible = true;
  tab.window_is_active = true;
  tab.window_show_state = 3;
  tab.window_tab_count = 127;
  tab.window_type = 4;

  return tab;
}

// Returns a tab features with all field set.
TabFeatures GetFullTabFeaturesForTesting() {
  TabFeatures tab;

  tab.has_before_unload_handler = true;
  tab.has_form_entry = true;
  tab.is_pinned = true;
  tab.key_event_count = 21;
  tab.mouse_event_count = 22;
  tab.mru_index = 27;
  tab.navigation_entry_count = 24;
  tab.num_reactivations = 25;
  tab.page_transition_core_type = 2;
  tab.page_transition_from_address_bar = true;
  tab.page_transition_is_redirect = true;
  tab.site_engagement_score = 26;
  tab.time_from_backgrounded = 10000;
  tab.total_tab_count = 30;
  tab.touch_event_count = 28;
  tab.was_recently_audible = true;
  tab.window_is_active = true;
  tab.window_show_state = 3;
  tab.window_tab_count = 27;
  tab.window_type = 4;

  tab.host = "www.google.com";
  tab.frecency_score = 0.1234f;
  return tab;
}

}  // namespace tab_ranker
