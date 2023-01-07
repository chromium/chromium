// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/undirected_graph.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(UndirectedGraphTest, AddUndirectedEdgeAndNodeWeight) {
  UndirectedGraph graph;
  graph.AddUndirectedEdgeAndNodeWeight(0, 1);
  graph.AddUndirectedEdgeAndNodeWeight(1, 0);
  graph.AddUndirectedEdgeAndNodeWeight(0, 2);
  graph.AddUndirectedEdgeAndNodeWeight(2, 3);
  graph.AddUndirectedEdgeAndNodeWeight(3, 2);

  EXPECT_EQ(graph.NumNodes(), (size_t)4);
  EXPECT_EQ(graph.NodeWeight(0), (size_t)3);
  EXPECT_EQ(graph.NodeWeight(1), (size_t)2);
  EXPECT_EQ(graph.NodeWeight(2), (size_t)3);
  EXPECT_EQ(graph.NodeWeight(3), (size_t)2);
  EXPECT_EQ(graph.total_node_weight(), (size_t)10);

  ASSERT_TRUE(graph.Neighbors(0).find(1) != graph.Neighbors(0).end());
  EXPECT_EQ(graph.Neighbors(0).at(1), 2);
  ASSERT_TRUE(graph.Neighbors(0).find(2) != graph.Neighbors(0).end());
  EXPECT_EQ(graph.Neighbors(0).at(2), 1);
}

}  // namespace ash