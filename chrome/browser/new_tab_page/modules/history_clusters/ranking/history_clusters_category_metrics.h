// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_CATEGORY_METRICS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_CATEGORY_METRICS_H_

#include <set>
#include <string>
#include <utility>

// Used for capturing aggregate cluster related metrics that will be used at
// cluster ranking time.
struct HistoryClustersCategoryMetrics {
  HistoryClustersCategoryMetrics();
  HistoryClustersCategoryMetrics(std::set<std::string> most_frequently_seen_ids,
                                 size_t most_frequent_seen_category_count,
                                 std::set<std::string> most_frequently_used_ids,
                                 size_t most_frequent_used_category_count);
  HistoryClustersCategoryMetrics(const HistoryClustersCategoryMetrics&);
  ~HistoryClustersCategoryMetrics();

  // A set with the most fequently seen category, or categories in case of a
  // tie, for a given period of time.
  std::set<std::string> most_frequently_seen_category_ids;

  size_t most_frequent_seen_category_for_period_count = 0;

  // A set with the most fequently used category, or categories in case of a
  // tie, for a given period of time.
  std::set<std::string> most_frequently_used_category_ids;

  size_t most_frequent_used_category_for_period_count = 0;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_CATEGORY_METRICS_H_
