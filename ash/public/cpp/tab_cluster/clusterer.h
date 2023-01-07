// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_CLUSTER_CLUSTERER_H_
#define ASH_PUBLIC_CPP_TAB_CLUSTER_CLUSTERER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tab_cluster/correlation_clusterer.h"
#include "ash/public/cpp/tab_cluster/undirected_graph.h"

namespace ash {

class TabClusterUIItem;

using TabItems = std::vector<std::unique_ptr<TabClusterUIItem>>;

struct ASH_PUBLIC_EXPORT ClusterResult {
  // The cluster_id of the node.
  int cluster_id = -1;
  // Boundary Strength of the cluster the node belong to, S = Iu/Eu
  // Iu is the sum of internal edge weights of a cluster
  // Eu is the sum of external edge weights of a cluster
  // Refer to go/bento-suggest-metrics
  double boundary_strength = 0.0;

  ClusterResult() = default;
  ~ClusterResult() = default;
  ClusterResult(const ClusterResult&) = default;
};

// The main class in charge of giving cluster result to `TabClusterUIController`
class ASH_PUBLIC_EXPORT Clusterer {
 public:
  Clusterer();
  ~Clusterer();

  // Updates Cluster info based on given information and returns
  // a list of tab items that are updated.
  std::vector<TabClusterUIItem*> GetUpdatedClusterInfo(
      const TabItems& tab_items,
      TabClusterUIItem* old_active_item,
      TabClusterUIItem* new_active_item);

 private:
  UndirectedGraph graph_;
  CorrelationClusterer correlation_clusterer_;
  std::map<std::string, size_t> source_to_node_;
  std::map<size_t, std::string> node_to_source_;

  // Clusters the current graph and return `ClusterResult` for all sources.
  std::map<std::string, ClusterResult> Cluster();
  // Adds tab switch as an edge in `graph_`.
  void AddEdge(const std::string& from_source, const std::string& to_source);
  // Returns the node_id of a given `source`, adds to `source_to_node_` if not
  // yet existed.
  size_t GetNodeForSource(const std::string& source);
  // Given a cluster of node_id, returns a cluster of sources.
  std::vector<std::string> GetSourcesFromCluster(std::vector<int> cluster);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TAB_CLUSTER_CLUSTERER_H_
