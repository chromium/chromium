// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_

#include "components/history_clusters/core/cluster_metrics_utils.h"
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
  // The HistoryClusters UI was opened indirectly; e.g., using an omnibox
  // action.
  kIndirectNavigation = 2,
  // The HistoryClusters UI was opened via a same-document navigation, which
  // means the user likely clicked the tab over from History to Journeys.
  kSameDocument = 3,
  // The Side Panel HistoryClusters UI was opened from the omnibox. Technically
  // this COULD be logged as kIndirectNavigation, but we want to be able to
  // distinguish between Side Panel and History WebUI initializations.
  kSidePanelFromOmnibox = 4,
  // The Side Panel HistoryClusters UI was opened from side panel toolbar
  // button.
  kSidePanelFromToolbarButton = 5,
  // Add new values above this line.
  kMaxValue = kSidePanelFromToolbarButton,
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

  std::optional<HistoryClustersInitialState> initial_state() const {
    return initial_state_;
  }

  void set_initial_state(HistoryClustersInitialState initial_state) {
    initial_state_ = initial_state;
  }

  void increment_query_count() { num_queries_++; }

  void increment_toggles_to_basic_history() { num_toggles_to_basic_history_++; }

  void set_navigation_id(int64_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  // Records that an |visit_action| in the UI occurred at |visit_index| position
  // on a specified |visit_type|.
  void RecordVisitAction(VisitAction visit_action,
                         uint32_t visit_index,
                         VisitType visit_type);

  // Records that a related search link was clicked at |related_search_index|.
  void RecordRelatedSearchAction(RelatedSearchAction action,
                                 uint32_t related_search_index);

  // Records that the journeys UI visibility was toggled.
  void RecordToggledVisibility(bool visible);

  // Records that an |cluster_action| in the UI occurred at |cluster_index|
  // position
  void RecordClusterAction(ClusterAction cluster_action,
                           uint32_t cluster_index);

  // Called when the UI becomes visible.
  void WasShown();

 private:
  // Whether the journeys interaction captured by |this| is considered a
  // successful outcome.
  bool IsCurrentlySuccessfulHistoryClustersOutcome();

  // The navigation ID of the navigation handle that this data is associated
  // with, used for recording the metrics to UKM.
  std::optional<int64_t> navigation_id_;

  // The initial state of how this interaction with the HistoryClusters UI was
  // started.
  std::optional<HistoryClustersInitialState> initial_state_;

  // True if the the HistoryClusters UI is ever shown. This can be false for the
  // entire lifetime of HistoryClusters UI if it is preloaded but never shown.
  bool is_ever_shown_ = false;

  // The number of queries made on the tracker history clusters event. Only
  // queries containing a string should be counted.
  int num_queries_ = 0;

  // The number of times in this interaction the user open a cluster or visit
  // link.
  int links_opened_count_ = 0;

  // The number of visits deleted from the HistoryClusters UI during |this|
  // interaction.
  int visits_deleted_count_ = 0;

  // The number of related search links clicked on a page tied associated with
  // |navigation_id|.
  int related_searches_click_count_ = 0;

  // The number of times a cluster of deleted on the journeys UI surface during
  // |this| interaction.
  int clusters_deleted_count_ = 0;

  // The number of times in this interaction with HistoryClusters included the
  // user toggled to the basic History UI from the HistoryClusters UI.
  int num_toggles_to_basic_history_ = 0;

  // The number of times the user toggled journeys off UI surface.
  int toggled_visiblity_count_ = 0;
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_METRICS_LOGGER_H_
