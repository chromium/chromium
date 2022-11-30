// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_CLUSTER_UNDIRECTED_GRAPH_H_
#define ASH_PUBLIC_CPP_TAB_CLUSTER_UNDIRECTED_GRAPH_H_

#include <map>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Data structure used for clustering.
// TODO(crbug/1231303): //ash/public mostly contains interfaces, shared
// constants, types and utility classes, which are used to share code between
// //ash and //chrome. It's not common to put all implementation inside
// //ash/public. Find a better place to move this class to.
class ASH_PUBLIC_EXPORT UndirectedGraph {
 public:
  UndirectedGraph();
  ~UndirectedGraph();
  UndirectedGraph(const UndirectedGraph& other);
  UndirectedGraph& operator=(const UndirectedGraph& other) = default;

  // Add node weight to both from_node and to_node.
  void AddUndirectedEdgeAndNodeWeight(size_t from_node, size_t to_node);
  size_t NumNodes() const;
  size_t NodeWeight(size_t id) const;
  const std::map<size_t, double>& Neighbors(size_t id) const;
  size_t total_node_weight() const { return total_node_weight_; }

 private:
  void AddEdge(size_t from_node, size_t to_node);
  void AddNodeWeight(size_t id);
  // Expand adjacency_lists_.
  void EnsureSize(size_t id);

  std::vector<std::map<size_t, double>> adjacency_lists_;
  std::vector<double> node_weights_;
  size_t total_node_weight_ = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TAB_CLUSTER_UNDIRECTED_GRAPH_H_
