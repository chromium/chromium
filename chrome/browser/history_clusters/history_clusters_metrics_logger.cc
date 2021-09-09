// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"

#include "base/logging.h"
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
  if (!init_state_)
    return;

  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(*navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::HistoryClusters builder(ukm_source_id);

  if (!final_state_) {
    // If `final_state_` is not set and `this` is being destructed,
    // we assume the UI is being closed either via tab or
    // window closing.
    final_state_ = HistoryClustersFinalState::kCloseTab;
  }
  builder.SetInitialState(static_cast<int>(*init_state_));
  builder.SetFinalState(static_cast<int>(*final_state_));
  builder.SetNumQueries(num_queries_);
  builder.SetNumTogglesToBasicHistory(num_toggles_to_basic_history_);
  builder.Record(ukm::UkmRecorder::Get());
}

PAGE_USER_DATA_KEY_IMPL(HistoryClustersMetricsLogger)

}  // namespace history_clusters
