// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/node_hash_cache.h"

#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
namespace {

TEST(NodeHashCacheTest, EmptyCache) {
  NodeHashCache cache;
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(0u, cache.size());

  StorageId id = base::UnguessableToken::Create();
  EXPECT_FALSE(cache.HasNode(id));
  EXPECT_EQ(std::nullopt, cache.GetNodeHashes(id));
}

TEST(NodeHashCacheTest, SetAndGetNodeHashes) {
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

TEST(NodeHashCacheTest, RemoveNode) {
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

TEST(NodeHashCacheTest, PruneExcept) {
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

TEST(NodeHashCacheTest, Clear) {
  NodeHashCache cache;
  StorageId id = base::UnguessableToken::Create();
  cache.SetNodeHashes(id, NodeHashes{1u, 2u});
  EXPECT_FALSE(cache.empty());

  cache.Clear();
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.HasNode(id));
}

TEST(NodeHashCacheTest, MoveSemantics) {
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

}  // namespace
}  // namespace tabs
