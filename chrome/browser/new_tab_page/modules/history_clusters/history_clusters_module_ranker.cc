// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_ranker.h"

#include "base/ranges/algorithm.h"
#include "components/history_clusters/core/history_clusters_util.h"

HistoryClustersModuleRanker::HistoryClustersModuleRanker(
    size_t max_clusters_to_return,
    const base::flat_set<std::string>& category_boostlist)
    : max_clusters_to_return_(max_clusters_to_return),
      category_boostlist_(category_boostlist) {}

HistoryClustersModuleRanker::~HistoryClustersModuleRanker() = default;

void HistoryClustersModuleRanker::RankClusters(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback) {
  // Within each cluster, sort visits.
  for (auto& cluster : clusters) {
    history_clusters::StableSortVisits(cluster.visits);
  }

  // After that, sort clusters based on params.
  base::ranges::stable_sort(clusters, [this](const auto& c1, const auto& c2) {
    if (c1.visits.empty()) {
      return false;
    }
    if (c2.visits.empty()) {
      return true;
    }

    // Boost categories if provided.
    if (!category_boostlist_.empty()) {
      bool c1_has_visit_in_categories =
          history_clusters::IsClusterInCategories(c1, category_boostlist_);
      bool c2_has_visit_in_categories =
          history_clusters::IsClusterInCategories(c2, category_boostlist_);

      if (c1_has_visit_in_categories ^ c2_has_visit_in_categories) {
        return c1_has_visit_in_categories;
      }
    }

    // Otherwise, fall back to reverse chronological.
    base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
    base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });

  if (max_clusters_to_return_ > 0 &&
      clusters.size() > max_clusters_to_return_) {
    clusters.resize(max_clusters_to_return_);
  }

  std::move(callback).Run(std::move(clusters));
}
