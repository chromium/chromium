// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/correlation_clusterer.h"

#include <map>
#include <optional>
#include <set>

#include "ash/public/cpp/tab_cluster/undirected_graph.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace ash {

namespace {

// Number of times we run the clustering algorithm on the given graph.
// This is an arbitrary number and might be subjected to further tuning.
constexpr int kNumIterations = 10;

// Converts current clustering into vector of vectors format.
std::vector<std::vector<int>> OutputClusters(
    const std::vector<int>& clustering) {
  std::map<int, std::vector<int>> clusters;
  for (size_t i = 0; i < clustering.size(); ++i) {
    clusters[clustering[i]].push_back(i);
  }
  std::vector<std::vector<int>> output;
  for (auto& key_value : clusters) {
    auto& cluster = key_value.second;
    output.push_back(std::move(cluster));
  }
  return output;
}
}  // namespace

// A helper class that keeps track of the sum of edge weights, accounting
// for missing edges, for best move computations.
class EdgeSum {
 public:
  EdgeSum() = default;
  EdgeSum(const EdgeSum&) = delete;
  EdgeSum& operator=(const EdgeSum&) = delete;
  ~EdgeSum() = default;

  // The edge weight `w` should have the edge weight offset subtracted before
  // calling this function.
  void Add(double w) { weight_ += w; }
  // Should be called at most once, after all edges have been Add()ed.
  void RemoveDoubleCounting() { weight_ /= 2.0; }
  // Retrieve the total weight of all edges seen, correcting for the implicit
  // negative weight of resolution multiplied by the product of the weights of
  // the two nodes incident to each edge.
  double NetWeight(
      double sum_prod_node_weights,
      const CorrelationClusterer::CorrelationClustererConfig& config) const {
    return weight_ - config.resolution * sum_prod_node_weights;
  }

 private:
  double weight_ = 0.0;
};

CorrelationClusterer::CorrelationClusterer() = default;
CorrelationClusterer::~CorrelationClusterer() = default;

std::vector<std::vector<int>> CorrelationClusterer::Cluster(
    const UndirectedGraph& undirected_graph) {
  Reset();

  graph_ = undirected_graph;
  num_nodes_ = graph_.NumNodes();

  // Create all-singletons initial clusters
  std::vector<std::vector<int>> clusters;

  for (int i = 0; i < num_nodes_; ++i) {
    clusters.push_back({i});
  }

  // Initialize to all-singletons clustering.
  clustering_.reserve(num_nodes_);
  for (int i = 0; i < num_nodes_; ++i) {
    int cluster = NewClusterId();
    clustering_.push_back(cluster);
    cluster_sizes_[cluster] = 1;
    cluster_weights_[cluster] = graph_.NodeWeight(i);
  }

  // Modularity objective.
  config_.resolution = 1.0 / graph_.total_node_weight();

  RefineClusters(&clusters);

  return clusters;
}

void CorrelationClusterer::RefineClusters(
    std::vector<std::vector<int>>* clusters_ptr) {
  std::string error;
  SetClustering(*clusters_ptr, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set clustering " << error;
    return;
  }

  double objective = 0;

  auto try_moves = [&](std::vector<std::set<int>>* clusters_to_try) {
    base::RandomShuffle(clusters_to_try->begin(), clusters_to_try->end());
    for (const auto& cluster : *clusters_to_try) {
      std::pair<std::optional<int>, double> best_move = BestMove(cluster);
      if (best_move.second > 0) {
        std::optional<int> new_cluster = best_move.first;
        MoveNodesToCluster(cluster, new_cluster);
        objective += best_move.second;
      }
    }
  };

  for (int iter = 0; iter < kNumIterations; ++iter) {
    // Use current clusters as move sets, which means we'll consider
    // merging clusters.
    std::map<int, std::set<int>> node_cluster_map;
    for (int i = 0; i < num_nodes_; ++i) {
      node_cluster_map[ClusterForNode(i)].insert(i);
    }
    std::vector<std::set<int>> temp_clusters;
    for (auto& key_value : node_cluster_map) {
      auto& cluster = key_value.second;
      temp_clusters.push_back(std::move(cluster));
    }
    try_moves(&temp_clusters);
  }

  *clusters_ptr = OutputClusters(clustering_);
}

bool CorrelationClusterer::SetClustering(
    const std::vector<std::vector<int>>& clusters,
    std::string* error) {
  std::vector<bool> seen_nodes(num_nodes_);
  for (const auto& cluster : clusters) {
    int id = NewClusterId();
    for (const auto node : cluster) {
      if (node >= num_nodes_ || node < 0) {
        *error =
            base::StrCat({"Node id ", base::NumberToString(node),
                          " in initial clusters not in expected range [0, ",
                          base::NumberToString(num_nodes_), ")"});
        return false;
      }
      if (seen_nodes[node]) {
        *error = base::StrCat({"Node id ", base::NumberToString(node),
                               " appears in initial clusters more than once."});
        return false;
      }
      seen_nodes[node] = true;
      MoveNodeToCluster(node, id);
    }
  }
  for (int node = 0; node < num_nodes_; ++node) {
    if (!seen_nodes[node]) {
      *error = base::StrCat({"Node id ", base::NumberToString(node),
                             " does not appear in initial clusters."});
      return false;
    }
  }
  return true;
}

void CorrelationClusterer::MoveNodeToCluster(const int node,
                                             const int new_cluster) {
  const int old_cluster = clustering_[node];
  const double weight = graph_.NodeWeight(node);
  cluster_sizes_[old_cluster] -= 1;
  cluster_weights_[old_cluster] -= weight;
  if (cluster_sizes_[old_cluster] == 0) {
    DCHECK_EQ(static_cast<int>(cluster_sizes_.erase(old_cluster)), 1);
    DCHECK_EQ(static_cast<int>(cluster_weights_.erase(old_cluster)), 1);
  }
  clustering_[node] = new_cluster;
  cluster_sizes_[new_cluster] += 1;
  cluster_weights_[new_cluster] += weight;
}

// Null optional means make a new cluster.
void CorrelationClusterer::MoveNodesToCluster(const std::set<int>& nodes,
                                              std::optional<int> new_cluster) {
  int actual_new_cluster = new_cluster ? *new_cluster : NewClusterId();
  for (const auto& node : nodes) {
    MoveNodeToCluster(node, actual_new_cluster);
  }
}

std::pair<std::optional<int>, double> CorrelationClusterer::BestMove(
    const std::set<int>& moving_nodes) {
  // Weight of nodes in each cluster that are moving.
  std::map<int, double> cluster_moving_weights;
  // Class 2 edges where the endpoints are currently in different clusters.
  EdgeSum class_2_currently_separate;
  // Class 1 edges where the endpoints are currently in the same cluster.
  EdgeSum class_1_currently_together;
  // Class 1 edges, grouped by the cluster that the non-moving node is in.
  std::map<int, EdgeSum> class_1_together_after;

  double moving_nodes_weight = 0;
  for (const auto& node : moving_nodes) {
    const int node_cluster = clustering_[node];
    cluster_moving_weights[node_cluster] += graph_.NodeWeight(node);
    moving_nodes_weight += graph_.NodeWeight(node);
    for (const auto& edge : graph_.Neighbors(node)) {
      const auto neighbor = edge.first;
      const auto weight = edge.second;
      const int neighbor_cluster = clustering_[neighbor];
      if (base::Contains(moving_nodes, neighbor)) {
        // Class 2 edge.
        if (node_cluster != neighbor_cluster) {
          class_2_currently_separate.Add(weight);
        }
      } else {
        // Class 1 edge.
        if (node_cluster == neighbor_cluster) {
          class_1_currently_together.Add(weight);
        }
        class_1_together_after[neighbor_cluster].Add(weight);
      }
    }
  }
  class_2_currently_separate.RemoveDoubleCounting();
  // Now cluster_moving_weights is correct and class_2_currently_separate,
  // class_1_currently_together, and class_1_by_cluster are ready to call
  // NetWeight().

  return BestMoveFromStats(moving_nodes_weight, cluster_moving_weights,
                           class_2_currently_separate,
                           class_1_currently_together, class_1_together_after);
}

std::pair<std::optional<int>, double> CorrelationClusterer::BestMoveFromStats(
    double moving_nodes_weight,
    std::map<int, double>& cluster_moving_weights,
    const EdgeSum& class_2_currently_separate,
    const EdgeSum& class_1_currently_together,
    const std::map<int, EdgeSum>& class_1_together_after) {
  double change_in_objective = 0.0;

  auto half_square = [](double x) { return x * x / 2.0; };
  double max_edges = half_square(moving_nodes_weight);
  for (const auto& cluster_moving_weight : cluster_moving_weights) {
    max_edges -= half_square(cluster_moving_weight.second);
  }
  change_in_objective +=
      class_2_currently_separate.NetWeight(max_edges, config_);

  max_edges = 0;
  for (const auto& cluster_moving_weight : cluster_moving_weights) {
    max_edges +=
        moving_nodes_weight * (GetClusterWeight(cluster_moving_weight.first) -
                               cluster_moving_weight.second);
  }
  change_in_objective -=
      class_1_currently_together.NetWeight(max_edges, config_);

  std::pair<std::optional<int>, double> best_move;
  best_move.first = std::nullopt;
  best_move.second = change_in_objective;
  for (const auto& cluster_data : class_1_together_after) {
    int cluster = cluster_data.first;
    const EdgeSum& data = cluster_data.second;
    max_edges = moving_nodes_weight *
                (GetClusterWeight(cluster) - cluster_moving_weights[cluster]);
    // Change in objective if we move the moving nodes to cluster i.
    double overall_change_in_objective =
        change_in_objective + data.NetWeight(max_edges, config_);
    if (overall_change_in_objective > best_move.second ||
        (overall_change_in_objective == best_move.second &&
         cluster < best_move.first)) {
      best_move.first = cluster;
      best_move.second = overall_change_in_objective;
    }
  }
  return best_move;
}

int CorrelationClusterer::NewClusterId() {
  return next_cluster_id_++;
}

int CorrelationClusterer::ClusterForNode(int node) const {
  return clustering_[node];
}

double CorrelationClusterer::GetClusterWeight(int cluster_id) const {
  return cluster_weights_.at(cluster_id);
}

void CorrelationClusterer::Reset() {
  clustering_.clear();
  cluster_sizes_.clear();
  cluster_weights_.clear();
  next_cluster_id_ = 0;
}

}  // namespace ash
