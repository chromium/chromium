// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTER_METRICS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTER_METRICS_H_

// Used for capturing cluster specific metrics that will be used at cluster
// ranking time.
struct HistoryClusterMetrics {
  // The number of times the user has seen the cluster for a given time range.
  size_t num_times_seen = 0;

  // The number of times the user has used the cluster for a given time range.
  // Usage in this context is defined as performing interactions on the UI
  // surface associated with the cluster that result in a navigation and certain
  // context menu actions.
  size_t num_times_used = 0;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTER_METRICS_H_
