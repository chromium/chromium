// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_RANKER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/history/core/browser/history_types.h"

// An object that sorts a list of clusters by likelihood of re-engagement.
class HistoryClustersModuleRanker {
 public:
  HistoryClustersModuleRanker(
      size_t max_clusters_to_return,
      const base::flat_set<std::string>& category_boostlist);
  ~HistoryClustersModuleRanker();

  // Sorts `clusters` by likelihood of re-engagement and invokes `callback` with
  // the top `n` clusters.
  using ClustersCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;
  void RankClusters(std::vector<history::Cluster> clusters,
                    ClustersCallback callback);

 private:
  // The max clusters to return.
  const size_t max_clusters_to_return_;

  // The category boostlist to use.
  const base::flat_set<std::string> category_boostlist_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_RANKER_H_
