// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_category_metrics.h"

#include <set>
#include <string>

HistoryClustersCategoryMetrics::HistoryClustersCategoryMetrics() = default;
HistoryClustersCategoryMetrics::HistoryClustersCategoryMetrics(
    std::set<std::string> most_frequently_seen_ids,
    size_t most_frequent_seen_category_count,
    std::set<std::string> most_frequently_engaged_ids,
    size_t most_frequent_used_category_count)
    : most_frequently_seen_category_ids(most_frequently_seen_ids),
      most_frequent_seen_category_for_period_count(
          most_frequent_seen_category_count),
      most_frequently_used_category_ids(most_frequently_engaged_ids),
      most_frequent_used_category_for_period_count(
          most_frequent_used_category_count) {}

HistoryClustersCategoryMetrics::HistoryClustersCategoryMetrics(
    const HistoryClustersCategoryMetrics&) = default;

HistoryClustersCategoryMetrics::~HistoryClustersCategoryMetrics() = default;
