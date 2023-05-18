// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"

#include "base/ranges/algorithm.h"
#include "components/history_clusters/core/history_clusters_util.h"

bool CompareClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& c1,
    const history::Cluster& c2) {
  if (c1.visits.empty()) {
    return false;
  }
  if (c2.visits.empty()) {
    return true;
  }

  // Boost categories if provided.
  if (!category_boostlist.empty()) {
    bool c1_has_visit_in_categories =
        history_clusters::IsClusterInCategories(c1, category_boostlist);
    bool c2_has_visit_in_categories =
        history_clusters::IsClusterInCategories(c2, category_boostlist);

    if (c1_has_visit_in_categories ^ c2_has_visit_in_categories) {
      return c1_has_visit_in_categories;
    }
  }

  // Otherwise, fall back to reverse chronological.
  base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
  base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

  // Use c1 > c2 to get more recent clusters BEFORE older clusters.
  return c1_time > c2_time;
}

void SortClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    std::vector<history::Cluster>& clusters) {
  base::ranges::stable_sort(clusters, [&](const auto& c1, const auto& c2) {
    return CompareClustersUsingHeuristic(category_boostlist, c1, c2);
  });
}
