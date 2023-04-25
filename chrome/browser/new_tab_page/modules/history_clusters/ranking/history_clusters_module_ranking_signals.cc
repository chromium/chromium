// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"

#include "components/history_clusters/core/history_clusters_util.h"

HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals(
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& cluster)
    : duration_since_most_recent_visit(
          base::Time::Now() -
          cluster.GetMostRecentVisit().annotated_visit.visit_row.visit_time),
      belongs_to_boosted_category(
          history_clusters::IsClusterInCategories(cluster,
                                                  category_boostlist)) {}
HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals() =
    default;
HistoryClustersModuleRankingSignals::~HistoryClustersModuleRankingSignals() =
    default;
HistoryClustersModuleRankingSignals::HistoryClustersModuleRankingSignals(
    const HistoryClustersModuleRankingSignals&) = default;
