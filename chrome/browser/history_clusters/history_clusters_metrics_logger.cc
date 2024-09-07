// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace history_clusters {

HistoryClustersMetricsLogger::HistoryClustersMetricsLogger(content::Page& page)
    : PageUserData<HistoryClustersMetricsLogger>(page) {}

HistoryClustersMetricsLogger::~HistoryClustersMetricsLogger() {
  if (!navigation_id_ || !initial_state_ || !is_ever_shown_) {
    return;
  }

  // Record UKM metrics.

  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(*navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::HistoryClusters builder(ukm_source_id);

  // TODO(crbug.com/40226001): Add UI-driven counts to UKM.
  builder.SetInitialState(static_cast<int>(*initial_state_));
  builder.SetNumQueries(num_queries_);
  builder.SetNumTogglesToBasicHistory(num_toggles_to_basic_history_);
  builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramEnumeration("History.Clusters.Actions.InitialState",
                                *initial_state_);

  // These metrics capture the total interactions on the page
  base::UmaHistogramCounts100(
      "History.Clusters.Actions.FinalState.NumberLinksOpened",
      links_opened_count_);
  base::UmaHistogramCounts100(
      "History.Clusters.Actions.FinalState.NumberRelatedSearchesClicked",
      related_searches_click_count_);
  base::UmaHistogramCounts100(
      "History.Clusters.Actions.FinalState.NumberVisibilityToggles",
      toggled_visiblity_count_);

  base::UmaHistogramCounts100(
      "History.Clusters.Actions.FinalState.NumberClustersDeleted",
      clusters_deleted_count_);
  base::UmaHistogramCounts100(
      "History.Clusters.Actions.FinalState.NumberIndividualVisitsDeleted",
      visits_deleted_count_);
  base::UmaHistogramBoolean("History.Clusters.Actions.FinalState.WasSuccessful",
                            IsCurrentlySuccessfulHistoryClustersOutcome());

  base::UmaHistogramBoolean("History.Clusters.Actions.DidMakeQuery",
                            num_queries_ > 0);
  if (num_queries_ > 0) {
    // Only log if at least 1 query was made so we measure the distribution of
    // the number of queries when made.
    base::UmaHistogramCounts100("History.Clusters.Actions.NumQueries",
                                num_queries_);
  }
}

void HistoryClustersMetricsLogger::RecordVisitAction(VisitAction visit_action,
                                                     uint32_t visit_index,
                                                     VisitType visit_type) {
  base::UmaHistogramCounts100(
      "History.Clusters.UIActions.Visit." + VisitActionToString(visit_action),
      visit_index);

  base::UmaHistogramCounts100("History.Clusters.UIActions." +
                                  VisitTypeToString(visit_type) + "Visit." +
                                  VisitActionToString(visit_action),
                              visit_index);
  if (visit_action == VisitAction::kClicked) {
    links_opened_count_++;
    return;
  }
  if (visit_action == VisitAction::kDeleted) {
    visits_deleted_count_++;
    return;
  }
}

void HistoryClustersMetricsLogger::RecordClusterAction(
    ClusterAction cluster_action,
    uint32_t cluster_index) {
  base::UmaHistogramCounts100("History.Clusters.UIActions.Cluster." +
                                  ClusterActionToString(cluster_action),
                              cluster_index);
  if (cluster_action == ClusterAction::kOpenedInTabGroup) {
    // ClusterAction::VisitClicked will have click counted in VisitAction.
    links_opened_count_++;
    return;
  }
  if (cluster_action == ClusterAction::kDeleted) {
    clusters_deleted_count_++;
    return;
  }
}

void HistoryClustersMetricsLogger::RecordRelatedSearchAction(
    RelatedSearchAction action,
    uint32_t related_search_index) {
  base::UmaHistogramCounts100("History.Clusters.UIActions.RelatedSearch." +
                                  RelatedSearchActionToString(action),
                              related_search_index);
  if (action == RelatedSearchAction::kClicked)
    related_searches_click_count_++;
}

void HistoryClustersMetricsLogger::RecordToggledVisibility(bool visible) {
  base::UmaHistogramBoolean("History.Clusters.UIActions.ToggledVisiblity",
                            visible);
  toggled_visiblity_count_++;
}

void HistoryClustersMetricsLogger::WasShown() {
  is_ever_shown_ = true;
}

bool HistoryClustersMetricsLogger::
    IsCurrentlySuccessfulHistoryClustersOutcome() {
  if (related_searches_click_count_ > 0)
    return true;
  if (links_opened_count_ > 0)
    return true;
  if (visits_deleted_count_ > 0)
    return true;
  if (clusters_deleted_count_ > 0)
    return true;

  return false;
}

PAGE_USER_DATA_KEY_IMPL(HistoryClustersMetricsLogger);

}  // namespace history_clusters
