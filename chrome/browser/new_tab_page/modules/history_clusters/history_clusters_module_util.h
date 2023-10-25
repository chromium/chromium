// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_split.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"

// Compares two clusters using heuristics.
bool CompareClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& c1,
    const history::Cluster& c2);

// Sorts clusters using `CompareClustersUsingHeuristic()` as the comparator.
void SortClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    std::vector<history::Cluster>& clusters);

// Creates filter params to be leveraged in by a `QueryClusters()` call.
history_clusters::QueryClustersFilterParams CreateFilterParamsFromFeatureFlags(
    int min_required_visits,
    int min_required_related_searches);

// Gets a list of categories associated for a given feature parameter.
base::flat_set<std::string> GetCategories(
    const char* feature_param,
    base::span<const std::string_view> default_categories);

// Gets the minimum number of images required in a cluster's visits to make it
// suitable for display in the UI.
int GetMinImagesToShow();

// Gets the maximum number of clusters that should ever be returned to the
// client.
size_t GetMaxClusters();

// Generates mock history clusters to be used for testing and demos of the
// module.
history::Cluster GenerateSampleCluster(int64_t cluster_id,
                                       int num_visits,
                                       int num_images);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_
