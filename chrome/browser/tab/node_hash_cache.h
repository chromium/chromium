// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_NODE_HASH_CACHE_H_
#define CHROME_BROWSER_TAB_NODE_HASH_CACHE_H_

#include <cstddef>
#include <optional>

#include "base/containers/span.h"
#include "chrome/browser/tab/storage_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

class StorageIdMapping;
class TabCollection;

struct NodeHashes {
  // Constant representing an empty or uninitialized hash value.
  static constexpr size_t kEmptyHash = 0;

  size_t payload_hash = kEmptyHash;
  size_t children_hash = kEmptyHash;

  bool operator==(const NodeHashes&) const = default;
};

// In-memory cache holding payload and children hashes for collection nodes.
// Enables delta-computed snapshots to bypass redundant SQLite writes.
class NodeHashCache {
 public:
  NodeHashCache();
  ~NodeHashCache();

  NodeHashCache(const NodeHashCache&) = delete;
  NodeHashCache& operator=(const NodeHashCache&) = delete;

  NodeHashCache(NodeHashCache&&);
  NodeHashCache& operator=(NodeHashCache&&);

  // Returns true if the node is present in the cache.
  bool HasNode(StorageId id) const;

  // Returns the cached hashes for the given node, if present.
  std::optional<NodeHashes> GetNodeHashes(StorageId id) const;

  // Sets the hashes for the given node.
  void SetNodeHashes(StorageId id, NodeHashes hashes);

  // Removes a node from the cache.
  void RemoveNode(StorageId id);

  // Removes all nodes from the cache except those in `live_ids`.
  void PruneExcept(base::span<const StorageId> live_ids);

  // Clears all cached hashes.
  void Clear();

  // Returns the number of cached nodes.
  size_t size() const;

  // Returns true if the cache is empty.
  bool empty() const;

  // Computes both payload and children hashes for the collection.
  static NodeHashes ComputeHashes(const TabCollection* collection,
                                  StorageIdMapping& mapping);

  // Computes the payload hash for the collection. Returns
  // NodeHashes::kEmptyHash for PINNED, UNPINNED, and TABSTRIP collections as
  // their payload state is either invariant or tracked externally.
  static size_t ComputePayloadHash(const TabCollection* collection);

  // Computes the children hash for direct children of the collection.
  static size_t ComputeChildrenHash(const TabCollection* collection,
                                    StorageIdMapping& mapping);

 private:
  absl::flat_hash_map<StorageId, NodeHashes> node_hashes_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_NODE_HASH_CACHE_H_
