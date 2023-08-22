// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/clusterer.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/public/cpp/tab_cluster/undirected_graph.h"
#include "base/containers/contains.h"
#include "base/logging.h"

namespace ash {

namespace {

// Returns true if the source is irrelevant to the clusterer.
bool ShouldSkip(const std::string& source) {
  // "about: and chrome:// are not recorded as host"
  return source.empty() || source == "blank" || source == "newtab/";
}

// Gets the list of cluster_id for nodes.
std::vector<int> GetNodeClusterId(std::vector<std::vector<int>> clusters,
                                  int num_nodes) {
  std::vector<int> node_cluster_id(num_nodes);
  for (size_t cluster_id = 0; cluster_id < clusters.size(); ++cluster_id) {
    for (int node_id : clusters[cluster_id]) {
      node_cluster_id[node_id] = cluster_id;
    }
  }
  return node_cluster_id;
}

double GetBoundaryStrength(UndirectedGraph& graph,
                           std::vector<int> cluster,
                           std::vector<int> node_cluster_id) {
  int internal_edge_weight = 0;
  int external_edge_weight = 0;
  for (int node : cluster) {
    for (const auto& edge : graph.Neighbors(node)) {
      const auto neighbor = edge.first;
      const auto weight = edge.second;

      if (node_cluster_id[neighbor] == node_cluster_id[node]) {
        internal_edge_weight += weight;
      } else {
        external_edge_weight += weight;
      }
    }
  }
  external_edge_weight = std::max(1, external_edge_weight);
  // Internal edge weight is accounted twice.
  // As internal edge weight and external edge weight are both int, divide
  // internal edge weight by 2.0 to get a double result if present.
  return (internal_edge_weight / 2.0) / external_edge_weight;
}

}  // namespace

Clusterer::Clusterer() = default;
Clusterer::~Clusterer() = default;

std::vector<TabClusterUIItem*> Clusterer::GetUpdatedClusterInfo(
    const TabItems& tab_items,
    TabClusterUIItem* old_active_item,
    TabClusterUIItem* new_active_item) {
  std::vector<TabClusterUIItem*> items;
  if (!old_active_item || !new_active_item)
    return items;

  const std::string& old_source = old_active_item->current_info().source;
  const std::string& new_source = new_active_item->current_info().source;
  // Ignores irrelevant sources and self-loop.
  if (ShouldSkip(old_source) || ShouldSkip(new_source) ||
      old_source == new_source)
    return items;

  AddEdge(old_source, new_source);

  // Get cluster results from the current graph.
  std::map<std::string, ClusterResult> result_map = Cluster();

  for (const auto& tab_item : tab_items) {
    const std::string& source = tab_item.get()->current_info().source;
    // Tab item source might not be present in result map yet when users have
    // yet to navigate between two tabs that are not ignored.
    if (!base::Contains(result_map, source)) {
      continue;
    }

    ClusterResult result = result_map.at(source);
    bool item_updated = false;
    if (tab_item->current_info().cluster_id != result.cluster_id) {
      tab_item->SetCurrentClusterId(result.cluster_id);
      item_updated = true;
    }
    if (tab_item->current_info().boundary_strength !=
        result.boundary_strength) {
      tab_item->SetCurrentBoundaryStrength(result.boundary_strength);
      item_updated = true;
    }
    if (item_updated) {
      items.push_back(tab_item.get());
    }
  }
  return items;
}

std::map<std::string, ClusterResult> Clusterer::Cluster() {
  std::vector<std::vector<int>> clusters =
      correlation_clusterer_.Cluster(graph_);

  std::vector<int> node_cluster_id =
      GetNodeClusterId(clusters, graph_.NumNodes());
  std::vector<double> boundary_strength_by_cluster_id;
  for (const auto& cluster : clusters) {
    boundary_strength_by_cluster_id.push_back(
        GetBoundaryStrength(graph_, cluster, node_cluster_id));
  }

  std::map<std::string, ClusterResult> result_map;
  for (size_t node = 0; node < node_cluster_id.size(); ++node) {
    ClusterResult result;
    result.cluster_id = node_cluster_id[node];
    result.boundary_strength =
        boundary_strength_by_cluster_id[result.cluster_id];
    result_map[node_to_source_.at(node)] = result;
  }

  return result_map;
}

void Clusterer::AddEdge(const std::string& from_source,
                        const std::string& to_source) {
  size_t from_node = GetNodeForSource(from_source);
  size_t to_node = GetNodeForSource(to_source);
  graph_.AddUndirectedEdgeAndNodeWeight(from_node, to_node);
}

size_t Clusterer::GetNodeForSource(const std::string& source) {
  if (base::Contains(source_to_node_, source)) {
    return source_to_node_.at(source);
  }
  size_t curr_size = source_to_node_.size();
  node_to_source_[curr_size] = source;
  source_to_node_[source] = curr_size;
  return curr_size;
}

std::vector<std::string> Clusterer::GetSourcesFromCluster(
    std::vector<int> cluster) {
  std::vector<std::string> sources;
  for (int node : cluster) {
    sources.push_back(node_to_source_.at(node));
  }
  return sources;
}

}  // namespace ash
