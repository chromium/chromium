// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/node_hash_cache.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/fake_tab_interface.h"
#include "components/tabs/public/mock_tab_group.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
namespace {

class FakeStorageIdMapping : public StorageIdMapping {
 public:
  StorageId GetStorageId(const TabCollection* collection) override {
    auto it = collection_ids_.find(collection);
    if (it == collection_ids_.end()) {
      StorageId id = base::UnguessableToken::Create();
      collection_ids_[collection] = id;
      return id;
    }
    return it->second;
  }

  StorageId GetStorageId(const TabInterface* tab) override {
    auto it = tab_ids_.find(tab);
    if (it == tab_ids_.end()) {
      StorageId id = base::UnguessableToken::Create();
      tab_ids_[tab] = id;
      return id;
    }
    return it->second;
  }

 private:
  std::map<const TabCollection*, StorageId> collection_ids_;
  std::map<const TabInterface*, StorageId> tab_ids_;
};

class NodeHashCacheTest : public ::testing::Test {
 public:
  NodeHashCacheTest() {
    ON_CALL(group_factory_, Create)
        .WillByDefault([](TabGroupTabCollection* collection,
                          const tab_groups::TabGroupId& id,
                          const tab_groups::TabGroupVisualData& visual_data) {
          return std::make_unique<MockTabGroup>(collection, id, visual_data);
        });
  }

  TabGroup::Factory& group_factory() { return group_factory_; }

 private:
  MockTabGroupFactory group_factory_{nullptr};
};

TEST_F(NodeHashCacheTest, EmptyCache) {
  NodeHashCache cache;
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(0u, cache.size());

  StorageId id = base::UnguessableToken::Create();
  EXPECT_FALSE(cache.HasNode(id));
  EXPECT_EQ(std::nullopt, cache.GetNodeHashes(id));
}

TEST_F(NodeHashCacheTest, SetAndGetNodeHashes) {
  NodeHashCache cache;
  StorageId id = base::UnguessableToken::Create();

  cache.SetNodeHashes(id, NodeHashes{12345u, 67890u});
  EXPECT_FALSE(cache.empty());
  EXPECT_EQ(1u, cache.size());
  EXPECT_TRUE(cache.HasNode(id));

  std::optional<NodeHashes> hashes = cache.GetNodeHashes(id);
  ASSERT_TRUE(hashes.has_value());
  EXPECT_EQ(12345u, hashes->payload_hash);
  EXPECT_EQ(67890u, hashes->children_hash);

  // Overwrite with updated NodeHashes.
  cache.SetNodeHashes(id, NodeHashes{111u, 222u});
  hashes = cache.GetNodeHashes(id);
  ASSERT_TRUE(hashes.has_value());
  EXPECT_EQ(111u, hashes->payload_hash);
  EXPECT_EQ(222u, hashes->children_hash);
}

TEST_F(NodeHashCacheTest, RemoveNode) {
  NodeHashCache cache;
  StorageId id1 = base::UnguessableToken::Create();
  StorageId id2 = base::UnguessableToken::Create();

  cache.SetNodeHashes(id1, NodeHashes{10u, 20u});
  cache.SetNodeHashes(id2, NodeHashes{30u, 40u});
  EXPECT_EQ(2u, cache.size());

  cache.RemoveNode(id1);
  EXPECT_EQ(1u, cache.size());
  EXPECT_FALSE(cache.HasNode(id1));
  EXPECT_TRUE(cache.HasNode(id2));

  // Removing non-existent node is a no-op.
  cache.RemoveNode(id1);
  EXPECT_EQ(1u, cache.size());
}

TEST_F(NodeHashCacheTest, PruneExcept) {
  NodeHashCache cache;
  StorageId id1 = base::UnguessableToken::Create();
  StorageId id2 = base::UnguessableToken::Create();
  StorageId id3 = base::UnguessableToken::Create();

  // Pruning an empty cache with non-empty live_ids is a safe no-op.
  std::vector<StorageId> live_ids = {id1, id3};
  cache.PruneExcept(live_ids);
  EXPECT_TRUE(cache.empty());

  cache.SetNodeHashes(id1, NodeHashes{1u, 2u});
  cache.SetNodeHashes(id2, NodeHashes{3u, 4u});
  cache.SetNodeHashes(id3, NodeHashes{5u, 6u});
  EXPECT_EQ(3u, cache.size());

  cache.PruneExcept(live_ids);

  EXPECT_EQ(2u, cache.size());
  EXPECT_TRUE(cache.HasNode(id1));
  EXPECT_FALSE(cache.HasNode(id2));
  EXPECT_TRUE(cache.HasNode(id3));

  // Pruning with extra IDs in live_ids that do not exist in cache preserves
  // matching entries.
  StorageId id_nonexistent = base::UnguessableToken::Create();
  cache.PruneExcept({id1, id_nonexistent});
  EXPECT_EQ(1u, cache.size());
  EXPECT_TRUE(cache.HasNode(id1));
  EXPECT_FALSE(cache.HasNode(id_nonexistent));

  // Pruning with empty live_ids clears all nodes.
  cache.PruneExcept({});
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(0u, cache.size());
}

TEST_F(NodeHashCacheTest, Clear) {
  NodeHashCache cache;
  StorageId id = base::UnguessableToken::Create();
  cache.SetNodeHashes(id, NodeHashes{1u, 2u});
  EXPECT_FALSE(cache.empty());

  cache.Clear();
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.HasNode(id));
}

TEST_F(NodeHashCacheTest, MoveSemantics) {
  NodeHashCache cache;
  StorageId id = base::UnguessableToken::Create();
  cache.SetNodeHashes(id, NodeHashes{42u, 84u});

  NodeHashCache moved_cache(std::move(cache));
  EXPECT_TRUE(moved_cache.HasNode(id));
  EXPECT_EQ(1u, moved_cache.size());
  EXPECT_EQ(moved_cache.GetNodeHashes(id), (NodeHashes{42u, 84u}));

  NodeHashCache assigned_cache;
  assigned_cache = std::move(moved_cache);
  EXPECT_TRUE(assigned_cache.HasNode(id));
  EXPECT_EQ(1u, assigned_cache.size());
  EXPECT_EQ(assigned_cache.GetNodeHashes(id), (NodeHashes{42u, 84u}));
}

TEST_F(NodeHashCacheTest, ComputePayloadHash_TabGroupCollection) {
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data(u"Initial Title",
                                             tab_groups::TabGroupColorId::kBlue,
                                             /*is_collapsed=*/false);
  TabGroupTabCollection group(group_factory(), group_id, visual_data);

  size_t base_hash = NodeHashCache::ComputePayloadHash(&group);
  EXPECT_NE(NodeHashes::kEmptyHash, base_hash);

  // Identical group ID and visual data produce the exact same payload hash.
  TabGroupTabCollection identical_group(group_factory(), group_id, visual_data);
  EXPECT_EQ(base_hash, NodeHashCache::ComputePayloadHash(&identical_group));

  // Different group ID with identical visual data produces a distinct hash.
  TabGroupTabCollection different_id_group(
      group_factory(), tab_groups::TabGroupId::GenerateNew(), visual_data);
  EXPECT_NE(base_hash, NodeHashCache::ComputePayloadHash(&different_id_group));

  // Changing title changes payload hash.
  group.GetTabGroup()->SetVisualData(tab_groups::TabGroupVisualData(
      u"Changed Title", tab_groups::TabGroupColorId::kBlue,
      /*is_collapsed=*/false));
  size_t changed_title_hash = NodeHashCache::ComputePayloadHash(&group);
  EXPECT_NE(base_hash, changed_title_hash);

  // Changing color changes payload hash.
  group.GetTabGroup()->SetVisualData(tab_groups::TabGroupVisualData(
      u"Changed Title", tab_groups::TabGroupColorId::kRed,
      /*is_collapsed=*/false));
  size_t changed_color_hash = NodeHashCache::ComputePayloadHash(&group);
  EXPECT_NE(changed_title_hash, changed_color_hash);

  // Changing collapsed state changes payload hash.
  group.GetTabGroup()->SetVisualData(tab_groups::TabGroupVisualData(
      u"Changed Title", tab_groups::TabGroupColorId::kRed,
      /*is_collapsed=*/true));
  size_t changed_collapsed_hash = NodeHashCache::ComputePayloadHash(&group);
  EXPECT_NE(changed_color_hash, changed_collapsed_hash);
}

TEST_F(NodeHashCacheTest, ComputePayloadHash_SplitTabCollection) {
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  split_tabs::SplitTabVisualData visual_data(
      split_tabs::SplitTabLayout::kSideBySide, 0.5);
  SplitTabCollection split(split_id, visual_data);

  size_t base_hash = NodeHashCache::ComputePayloadHash(&split);
  EXPECT_NE(NodeHashes::kEmptyHash, base_hash);

  // Identical split ID and visual data produce the exact same payload hash.
  SplitTabCollection identical_split(split_id, visual_data);
  EXPECT_EQ(base_hash, NodeHashCache::ComputePayloadHash(&identical_split));

  // Different split ID with identical visual data produces a distinct hash.
  SplitTabCollection different_id_split(split_tabs::SplitTabId::GenerateNew(),
                                        visual_data);
  EXPECT_NE(base_hash, NodeHashCache::ComputePayloadHash(&different_id_split));

  // Changing split ratio changes payload hash.
  split.data()->visual_data()->set_split_ratio(0.7);
  size_t changed_ratio_hash = NodeHashCache::ComputePayloadHash(&split);
  EXPECT_NE(base_hash, changed_ratio_hash);

  // Changing layout changes payload hash.
  split.data()->visual_data()->set_split_layout(
      split_tabs::SplitTabLayout::kStacked);
  size_t changed_layout_hash = NodeHashCache::ComputePayloadHash(&split);
  EXPECT_NE(changed_ratio_hash, changed_layout_hash);
}

TEST_F(NodeHashCacheTest, ComputePayloadHash_InvariantCollections) {
  TabStripCollection tab_strip;
  EXPECT_EQ(NodeHashes::kEmptyHash,
            NodeHashCache::ComputePayloadHash(&tab_strip));

  PinnedTabCollection pinned;
  EXPECT_EQ(NodeHashes::kEmptyHash, NodeHashCache::ComputePayloadHash(&pinned));

  UnpinnedTabCollection unpinned;
  EXPECT_EQ(NodeHashes::kEmptyHash,
            NodeHashCache::ComputePayloadHash(&unpinned));
}

TEST_F(NodeHashCacheTest, ComputeChildrenHash_DirectChildren) {
  FakeStorageIdMapping mapping;

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data(
      u"Group", tab_groups::TabGroupColorId::kBlue, /*is_collapsed=*/false);
  TabGroupTabCollection group(group_factory(), group_id, visual_data);

  size_t empty_hash = NodeHashCache::ComputeChildrenHash(&group, mapping);
  EXPECT_EQ(NodeHashes::kEmptyHash, empty_hash);

  auto tab1 = std::make_unique<FakeTabInterface>();
  FakeTabInterface* tab1_ptr = tab1.get();
  group.AddTab(std::move(tab1), 0);

  size_t one_tab_hash = NodeHashCache::ComputeChildrenHash(&group, mapping);
  EXPECT_NE(NodeHashes::kEmptyHash, one_tab_hash);

  auto tab2 = std::make_unique<FakeTabInterface>();
  group.AddTab(std::move(tab2), 1);

  size_t two_tabs_hash = NodeHashCache::ComputeChildrenHash(&group, mapping);
  EXPECT_NE(one_tab_hash, two_tabs_hash);

  // Reordering children changes the children hash.
  auto removed_tab = group.MaybeRemoveTab(tab1_ptr);
  ASSERT_NE(nullptr, removed_tab);
  group.AddTab(std::move(removed_tab), 1);
  size_t reordered_hash = NodeHashCache::ComputeChildrenHash(&group, mapping);
  EXPECT_NE(two_tabs_hash, reordered_hash);
}

TEST_F(NodeHashCacheTest, ComputeChildrenHash_ChildCollections) {
  FakeStorageIdMapping mapping;
  UnpinnedTabCollection unpinned;

  size_t empty_hash = NodeHashCache::ComputeChildrenHash(&unpinned, mapping);
  EXPECT_EQ(NodeHashes::kEmptyHash, empty_hash);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data(
      u"Group", tab_groups::TabGroupColorId::kBlue, /*is_collapsed=*/false);
  auto group = std::make_unique<TabGroupTabCollection>(group_factory(),
                                                       group_id, visual_data);
  group->AddTab(std::make_unique<FakeTabInterface>(), 0);

  unpinned.AddCollection(std::move(group), 0);

  size_t one_collection_hash =
      NodeHashCache::ComputeChildrenHash(&unpinned, mapping);
  EXPECT_NE(NodeHashes::kEmptyHash, one_collection_hash);

  // Adding a second collection changes the children hash.
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  split_tabs::SplitTabVisualData split_visual(
      split_tabs::SplitTabLayout::kSideBySide, 0.5);
  auto split = std::make_unique<SplitTabCollection>(split_id, split_visual);
  split->AddTab(std::make_unique<FakeTabInterface>(), 0);
  unpinned.AddCollection(std::move(split), 1);

  size_t two_collections_hash =
      NodeHashCache::ComputeChildrenHash(&unpinned, mapping);
  EXPECT_NE(one_collection_hash, two_collections_hash);
}

TEST_F(NodeHashCacheTest, ComputeHashes_Combined) {
  FakeStorageIdMapping mapping;

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data(
      u"Group", tab_groups::TabGroupColorId::kBlue, /*is_collapsed=*/false);
  TabGroupTabCollection group(group_factory(), group_id, visual_data);
  group.AddTab(std::make_unique<FakeTabInterface>(), 0);

  NodeHashes hashes = NodeHashCache::ComputeHashes(&group, mapping);
  EXPECT_EQ(NodeHashCache::ComputePayloadHash(&group), hashes.payload_hash);
  EXPECT_EQ(NodeHashCache::ComputeChildrenHash(&group, mapping),
            hashes.children_hash);
  EXPECT_NE(NodeHashes::kEmptyHash, hashes.payload_hash);
  EXPECT_NE(NodeHashes::kEmptyHash, hashes.children_hash);
}

}  // namespace
}  // namespace tabs
