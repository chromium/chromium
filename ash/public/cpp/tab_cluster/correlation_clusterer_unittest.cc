// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/correlation_clusterer.h"

#include "ash/public/cpp/tab_cluster/undirected_graph.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::UnorderedElementsAreArray;

// TODO(b/190004922): Update the algorithm so that a fully connected graph
// with all edge weights = 1 should only produce one cluster.

TEST(CorrelationClusterer, TwoDistinctClusters) {
  CorrelationClusterer clusterer;
  UndirectedGraph graph;
  graph.AddUndirectedEdgeAndNodeWeight(0, 1);
  graph.AddUndirectedEdgeAndNodeWeight(1, 0);
  graph.AddUndirectedEdgeAndNodeWeight(0, 2);
  graph.AddUndirectedEdgeAndNodeWeight(2, 3);
  graph.AddUndirectedEdgeAndNodeWeight(3, 2);
  std::vector<std::vector<int>> clusters = clusterer.Cluster(graph);
  EXPECT_THAT(clusters,
              UnorderedElementsAreArray<std::vector<int>>({{0, 1}, {2, 3}}));
}

TEST(CorrelationClusterer, EmptyClusterWhenGraphIsEmpty) {
  CorrelationClusterer clusterer;
  UndirectedGraph graph;
  std::vector<std::vector<int>> clusters = clusterer.Cluster(graph);
  EXPECT_TRUE(clusters.empty());
}

TEST(CorrelationClusterer, OneClusterWhenAllNodesIsConnectedToANode) {
  CorrelationClusterer clusterer;
  UndirectedGraph graph;
  graph.AddUndirectedEdgeAndNodeWeight(0, 1);
  graph.AddUndirectedEdgeAndNodeWeight(0, 2);
  graph.AddUndirectedEdgeAndNodeWeight(0, 3);
  std::vector<std::vector<int>> clusters = clusterer.Cluster(graph);
  EXPECT_THAT(clusters,
              UnorderedElementsAreArray<std::vector<int>>({{0, 1, 2, 3}}));
}

}  // namespace ash