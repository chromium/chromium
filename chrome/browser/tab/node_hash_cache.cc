// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/node_hash_cache.h"

#include <utility>

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

NodeHashCache::NodeHashCache() = default;
NodeHashCache::~NodeHashCache() = default;

NodeHashCache::NodeHashCache(NodeHashCache&&) = default;
NodeHashCache& NodeHashCache::operator=(NodeHashCache&&) = default;

bool NodeHashCache::HasNode(StorageId id) const {
  return node_hashes_.contains(id);
}

std::optional<NodeHashes> NodeHashCache::GetNodeHashes(StorageId id) const {
  const NodeHashes* hashes = base::FindOrNull(node_hashes_, id);
  return hashes ? std::make_optional(*hashes) : std::nullopt;
}

void NodeHashCache::SetNodeHashes(StorageId id, NodeHashes hashes) {
  node_hashes_[id] = hashes;
}

void NodeHashCache::RemoveNode(StorageId id) {
  node_hashes_.erase(id);
}

void NodeHashCache::PruneExcept(base::span<const StorageId> live_ids) {
  if (node_hashes_.empty()) {
    return;
  }
  if (live_ids.empty()) {
    node_hashes_.clear();
    return;
  }
  absl::flat_hash_set<StorageId> live_set(live_ids.begin(), live_ids.end());
  absl::erase_if(node_hashes_, [&live_set](const auto& entry) {
    return !live_set.contains(entry.first);
  });
}

void NodeHashCache::Clear() {
  node_hashes_.clear();
}

size_t NodeHashCache::size() const {
  return node_hashes_.size();
}

bool NodeHashCache::empty() const {
  return node_hashes_.empty();
}

}  // namespace tabs
