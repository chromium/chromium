// Copyright 2021 The Chromium Authors. All rights reserved.
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
  if (!navigation_id_)
    return;
  if (!initial_state_)
    return;

  // Record UKM metrics.

  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(*navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::HistoryClusters builder(ukm_source_id);

  if (!final_state_) {
    // If `final_state_` is not set and `this` is being destructed,
    // we assume the UI is being closed either via tab or
    // window closing.
    final_state_ = HistoryClustersFinalState::kCloseTab;
  }
  // TODO(crbug.com/1326954): Add UI-driven counts to UKM during final state
  // clean up.
  builder.SetInitialState(static_cast<int>(*initial_state_));
  builder.SetFinalState(static_cast<int>(*final_state_));
  builder.SetNumQueries(num_queries_);
  builder.SetNumTogglesToBasicHistory(num_toggles_to_basic_history_);
  builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramEnumeration("History.Clusters.Actions.InitialState",
                                *initial_state_);

  // TODO(crbug.com/1326954): Clean up once the metrics are refactored
  // to be driven by UI events rather than navigation state. Also
  // add collected counts to both UMA and UKM.
  base::UmaHistogramEnumeration("History.Clusters.Actions.FinalState",
                                *final_state_);

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
  // TODO(crbug.com/1326954): Add final state of total counts for each visit
  // action.
}

void HistoryClustersMetricsLogger::RecordClusterAction(
    ClusterAction cluster_action,
    uint32_t cluster_index) {
  base::UmaHistogramCounts100("History.Clusters.UIActions.Cluster." +
                                  ClusterActionToString(cluster_action),
                              cluster_index);
  // TODO(crbug.com/1326954): Add final state of total counts for each cluster
  // action.
}

void HistoryClustersMetricsLogger::RecordRelatedSearchAction(
    RelatedSearchAction action,
    uint32_t related_search_index) {
  base::UmaHistogramCounts100("History.Clusters.UIActions.RelatedSearch." +
                                  RelatedSearchActionToString(action),
                              related_search_index);
  // TODO(crbug.com/1326954): Add final state of total counts for each related
  // search action.
}

void HistoryClustersMetricsLogger::RecordToggledVisibility(bool visible) {
  base::UmaHistogramBoolean("History.Clusters.UIActions.ToggledVisiblity",
                            visible);
  // TODO(crbug.com/1326954): Add final state of total counts for each
  // visibility toggle.
}

PAGE_USER_DATA_KEY_IMPL(HistoryClustersMetricsLogger);

}  // namespace history_clusters
