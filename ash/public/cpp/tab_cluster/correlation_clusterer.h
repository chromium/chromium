// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_CLUSTER_CORRELATION_CLUSTERER_H_
#define ASH_PUBLIC_CPP_TAB_CLUSTER_CORRELATION_CLUSTERER_H_

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tab_cluster/undirected_graph.h"

namespace ash {

class UndirectedGraph;
class EdgeSum;

// Adapted from
// https://github.com/google-research/google-research/blob/HEAD/parallel_clustering/clustering/config.proto#L66
// Consider a graph with vertex set V, edge set E, non-negative vertex weights
// k_u, edge weights w_uv, and a "resolution" parameter which must be
// non-negative. We define "rescaled" edge weights w'_uv for all u, v, in V as:
//             { 0                                if u == v
// w'_{uv} =   {  w_uv  -  resolution k_u k_v     if {u, v} in E,
//             {  -resolution k_u k_v             otherwise
// The correlation clustering objective is to maximize
//   sum_{u, v in the same cluster} w'_uv,
// which is equivalent (up to sign and an additive constant) to the
// "maximizing agreements" and "minimizing disagreements" formulations of
// correlation clustering that are used in the approximation algorithms
// literature. Assuming the total edge weight in the graph is M, modularity
// partitioning can be expressed in this form by:
//  * setting resolution = 1/(2*M),
//  * setting the weight of each node to the total weight of its incident edges.
// Note that the final correlation clustering objective is monotonic in, but not
// equal to modularity. In particular, if the correlation clustering objective
// is C, we have:
// modularity = (C - resolution * sum_v (deg_v)^2 / (4 * M)) / M.
//
// To optimize this objective we use local search. We start with each vertex in
// its own cluster. We consider moves of the following form: move all vertices
// in a "move set" S of vertices to either one of the existing clusters or to a
// newly created cluster. We currently consider the following options for S:
//  - One move set per current cluster with all the vertices currently in it.
//    With these move sets we're effectively considering merging two clusters.
// For each move set considered we move that move set to the cluster that
// improves the objective the most if an improving move exists.
// TODO(crbug/1231303): //ash/public mostly contains interfaces, shared
// constants, types and utility classes, which are used to share code between
// //ash and //chrome. It's not common to put all implementation inside
// //ash/public. Find a better place to move this class to.
class ASH_PUBLIC_EXPORT CorrelationClusterer {
 public:
  struct CorrelationClustererConfig {
    double resolution = 1.0;
  };
  CorrelationClusterer();
  CorrelationClusterer(const CorrelationClusterer&) = delete;
  CorrelationClusterer& operator=(const CorrelationClusterer&) = delete;
  ~CorrelationClusterer();

  // Returns a list of clusters for a given undirected graph.
  std::vector<std::vector<int>> Cluster(
      const UndirectedGraph& undirected_graph);

 private:
  // The main clustering function.
  // initial_clustering must include every node in the range
  // [0, MutableGraph().NumNodes()) exactly once. If it doesn't this function
  // will log an error message and return clusters of singleton.
  void RefineClusters(std::vector<std::vector<int>>* clusters_ptr);
  // Fills `clustering_` with a map of node (index) to cluster id (value)
  // from the given set of `clusters`, returns false with non-empty
  // error string when a problem occurred.
  bool SetClustering(const std::vector<std::vector<int>>& clusters,
                     std::string* error);
  // Moves node from its current cluster to a new cluster.
  void MoveNodeToCluster(const int node, const int new_cluster);
  void MoveNodesToCluster(const std::set<int>& nodes,
                          std::optional<int> new_cluster);
  // Returns a pair of:
  //  * The best cluster to move `moving_nodes` to according to the correlation
  //    clustering objective function. Null optional means create a new cluster.
  //  * The change in objective function achieved by that move. May be positive
  //    or negative.
  // The runtime is linear in the number of edges incident to `moving_nodes`.
  std::pair<std::optional<int>, double> BestMove(
      const std::set<int>& moving_nodes);
  // Computes the best move given certain pre-computed sums of edge weights of
  // the following classes of vertices in relation to a fixed set of
  // `moving_nodes` that may change clusters:
  //  * Class 0: Neither node is moving.
  //  * Class 1: Exactly one node is moving.
  //  * Class 2: Both nodes are moving.
  // where "moving" means in moving_nodes.
  //
  // Change in objective if we move all `moving nodes` to cluster i:
  //   class_2_currently_separate + class_1_together_after[i] -
  //   class_1_currently_together
  // where
  //   class_2_currently_separate = Weight of edges in class 2 where the
  //   endpoints are in different clusters currently
  //   class_1_together_after[i] = Weight of edges in class 1 where the
  //   non-moving node is in cluster i
  //   class_1_currently_together = Weight of edges in class 1 where the
  //   endpoints are in the same cluster currently
  //
  // Two complications:
  //   * We need to avoid double-counting pairs in class 2
  //   * We need to account for missing edges, which have weight
  //     -resolution. To do so we subtract the number of edges we see in each
  //     category from the max possible number of edges (i.e. the number of
  //     edges we'd have if the graph was complete).
  std::pair<std::optional<int>, double> BestMoveFromStats(
      double moving_nodes_weight,
      std::map<int, double>& cluster_moving_weights,
      const EdgeSum& class_2_currently_separate,
      const EdgeSum& class_1_currently_together,
      const std::map<int, EdgeSum>& class_1_together_after);
  // Increments `next_cluster_id_`.
  int NewClusterId();
  // Gets the cluster of a given `node` from `clustering_`.
  int ClusterForNode(int node) const;
  // Gets cluster weight of a `cluster_id` from `cluster_weights_` map.
  double GetClusterWeight(int cluster_id) const;
  // Resets data members before performing clustering.
  void Reset();

  UndirectedGraph graph_;
  CorrelationClustererConfig config_;
  // Maps node to its cluster.
  std::vector<int> clustering_;
  // Number of nodes in each cluster.
  // Currently this is used only to detect when a cluster is empty so its
  // entry can be removed from cluster_sizes_ and cluster_weights_.
  std::map<int, int32_t> cluster_sizes_;
  // Sum of the weights of the nodes in each cluster.
  std::map<int, double> cluster_weights_;
  int next_cluster_id_ = 0;
  int num_nodes_ = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TAB_CLUSTER_CORRELATION_CLUSTERER_H_
