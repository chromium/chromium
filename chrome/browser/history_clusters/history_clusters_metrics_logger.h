// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_

#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace history_clusters {

// The initial state that describes how an interaction with the HistoryClusters
// UI was started.
//
// Keep in sync with HistoryClustersInitialState in
// tools/metrics/histograms/enums.xml.
enum class HistoryClustersInitialState {
  kUnknown = 0,
  // The HistoryClusters UI was opened via direct URL, i.e., not opened via any
  // other surface/path such as an omnibox action or other UI surface.
  kDirectNavigation = 1,
  // The HistoryClusters UI was opened indirectly; e.g., using the link the
  // chrome://history sidebar.
  kIndirectNavigation = 2,
  // Add new values above this line.
  kMaxValue = kIndirectNavigation,
};

// The final state, or outcome, of an interaction on the HistoryClusters UI.
//
// Keep in sync with HistoryClustersFinalState in
// tools/metrics/histograms/enums.xml.
enum class HistoryClustersFinalState {
  kUnknown = 0,
  // The interaction with the HistoryClusters UI ended with a click on a link.
  kLinkClick = 1,
  // The UI interaction ended without opening anything on the page.
  kCloseTab = 2,
  // The interaction ended with a same doc navigation; i.e., the
  // 'Chrome history' & 'Tabs from other devices' links. Because a user may
  // toggle between the history UIs, `kSameDocNavigation` is only used if the
  // user was not on the HistoryClusters UI last. E.g., 1) navigating to the
  // HistoryClustersUi, 2) toggling to the history UI, 3) returning to the
  // HistoryClustersUI, and 4) closing the tab will record `kCloseTab`, not
  // `kSameDocNavigation`.
  kSameDocNavigation = 3,
  // The interaction ended with a page refresh.
  kRefreshTab = 4,
  // Add new values above this line.
  kMaxValue = kRefreshTab,
};

// HistoryClustersMetricsLogger contains all the metrics/events associated with
// interactions and internals of HistoryClusters in Chrome. It has the same
// lifetime as the page's main document and metrics are flushed when `this` is
// destructed.
class HistoryClustersMetricsLogger
    : public content::PageUserData<HistoryClustersMetricsLogger> {
 public:
  explicit HistoryClustersMetricsLogger(content::Page& page);
  ~HistoryClustersMetricsLogger() override;
  PAGE_USER_DATA_KEY_DECL();

  void set_initial_state(HistoryClustersInitialState init_state) {
    init_state_ = init_state;
  }

  absl::optional<HistoryClustersFinalState> get_final_state() {
    return final_state_;
  }

  void set_final_state(HistoryClustersFinalState final_state) {
    final_state_ = final_state;
  }

  void clear_final_state() { final_state_.reset(); }

  void increment_query_count() { num_queries_++; }

  void increment_toggles_to_basic_history() { num_toggles_to_basic_history_++; }

  void set_navigation_id(int64_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  void IncrementLinksOpenedCount() { links_opened_count_++; }

 private:
  // The navigation ID of the navigation handle that this data is associated
  // with, used for recording the metrics to UKM.
  absl::optional<int64_t> navigation_id_;

  // The initial state of how this interaction with the HistoryClusters UI was
  // started.
  absl::optional<HistoryClustersInitialState> init_state_;

  // The final state of how this interaction with the HistoryClusters UI ended.
  absl::optional<HistoryClustersFinalState> final_state_;

  // The number of queries made on the tracker history clusters event. Only
  // queries containing a string should be counted.
  int num_queries_ = 0;

  // The number of times in this interaction with HistoryClusters included the
  // user toggled to the basic History UI from the HistoryClusters UI.
  int num_toggles_to_basic_history_ = 0;

  // The number of links opened from the HistoryClusters UI. Includes both
  // same-tab and new-tab/window navigations. Includes both visit and related
  // search links. Does not include sidebar navigations (e.g. 'Clear browsing
  // data').
  int links_opened_count_ = 0;
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_
