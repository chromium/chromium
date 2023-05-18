// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/history/core/browser/history_types.h"

// Compares two clusters using heuristics.
bool CompareClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    const history::Cluster& c1,
    const history::Cluster& c2);

// Sorts clusters using `CompareClustersUsingHeuristic()` as the comparator.
void SortClustersUsingHeuristic(
    const base::flat_set<std::string>& category_boostlist,
    std::vector<history::Cluster>& clusters);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_UTIL_H_
