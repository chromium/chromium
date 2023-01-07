// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/undirected_graph.h"

#include "base/check.h"

namespace ash {

UndirectedGraph::UndirectedGraph() = default;
UndirectedGraph::~UndirectedGraph() = default;
UndirectedGraph::UndirectedGraph(const UndirectedGraph&) = default;

void UndirectedGraph::AddEdge(size_t from_node, size_t to_node) {
  EnsureSize(from_node);
  EnsureSize(to_node);
  ++adjacency_lists_[from_node][to_node];
}

void UndirectedGraph::AddNodeWeight(size_t id) {
  EnsureSize(id);
  if (id >= node_weights_.size()) {
    node_weights_.resize(id + 1, 0.0);
  }
  ++node_weights_[id];
  ++total_node_weight_;
}

void UndirectedGraph::AddUndirectedEdgeAndNodeWeight(size_t from_node,
                                                     size_t to_node) {
  AddEdge(from_node, to_node);
  AddEdge(to_node, from_node);
  AddNodeWeight(from_node);
  AddNodeWeight(to_node);
}

void UndirectedGraph::EnsureSize(size_t id) {
  if (static_cast<size_t>(adjacency_lists_.size()) < id + 1) {
    adjacency_lists_.resize(id + 1);
  }
}

size_t UndirectedGraph::NumNodes() const {
  return adjacency_lists_.size();
}

size_t UndirectedGraph::NodeWeight(size_t id) const {
  DCHECK(id >= 0 && id < NumNodes());
  return node_weights_[id];
}

const std::map<size_t, double>& UndirectedGraph::Neighbors(size_t id) const {
  DCHECK(id >= 0 && id < NumNodes());
  return adjacency_lists_[id];
}

}  // namespace ash