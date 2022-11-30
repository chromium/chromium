// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tab_cluster/clusterer.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/public/cpp/tab_cluster/undirected_graph.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

std::unique_ptr<TabClusterUIItem> CreateTabClusterUIItem(
    const std::string& source) {
  ash::TabClusterUIItem::Info info;
  info.source = source;
  return std::make_unique<TabClusterUIItem>(info);
}

}  // namespace

TEST(Clusterer, GetUpdatedClusterInfo) {
  auto item_1 = CreateTabClusterUIItem("1");
  auto item_2 = CreateTabClusterUIItem("2");

  TabItems items;
  items.push_back(std::move(item_1));
  items.push_back(std::move(item_2));

  Clusterer clusterer;
  clusterer.GetUpdatedClusterInfo(items, items[0].get(), items[1].get());
  clusterer.GetUpdatedClusterInfo(items, items[1].get(), items[0].get());
  auto updated_items =
      clusterer.GetUpdatedClusterInfo(items, items[0].get(), items[1].get());

  EXPECT_EQ(updated_items.size(), size_t(2));
  EXPECT_EQ(updated_items[0]->current_info().cluster_id,
            updated_items[1]->current_info().cluster_id);
  EXPECT_EQ(updated_items[0]->current_info().boundary_strength,
            updated_items[1]->current_info().boundary_strength);
  EXPECT_EQ(updated_items[0]->current_info().boundary_strength, 3.0);
}

TEST(Clusterer, GetEmptyUpdatedClusterInfoAsSourcesAreSkipped) {
  auto item_1 = CreateTabClusterUIItem("");
  auto item_2 = CreateTabClusterUIItem("");

  TabItems items;
  items.push_back(std::move(item_1));
  items.push_back(std::move(item_2));

  Clusterer clusterer;
  clusterer.GetUpdatedClusterInfo(items, items[0].get(), items[1].get());
  clusterer.GetUpdatedClusterInfo(items, items[1].get(), items[0].get());
  auto updated_items =
      clusterer.GetUpdatedClusterInfo(items, items[0].get(), items[1].get());

  EXPECT_EQ(updated_items.size(), size_t(0));
}

}  // namespace ash
