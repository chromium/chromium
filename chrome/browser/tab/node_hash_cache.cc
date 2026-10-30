// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/node_hash_cache.h"

#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ref.h"
#include "base/token.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

namespace {

size_t HashTabGroupCollection(const TabGroupTabCollection* collection) {
  CHECK(collection);
  const TabGroup* group = collection->GetTabGroup();
  if (!group) {
    return NodeHashes::kEmptyHash;
  }
  const base::Token token = group->id().token();
  const tab_groups::TabGroupVisualData* visual_data = group->visual_data();
  if (!visual_data) {
    return base::HashCombine(NodeHashes::kEmptyHash, token.high(), token.low());
  }
  return base::HashCombine(
      NodeHashes::kEmptyHash, token.high(), token.low(),
      std::to_underlying(visual_data->color()), visual_data->is_collapsed(),
      base::FastHash(base::as_byte_span(visual_data->title())));
}

size_t HashSplitTabCollection(const SplitTabCollection* collection) {
  CHECK(collection);
  const split_tabs::SplitTabData* data = collection->data();
  if (!data) {
    return NodeHashes::kEmptyHash;
  }
  const base::Token token = data->id().token();
  const split_tabs::SplitTabVisualData* visual_data = data->visual_data();
  if (!visual_data) {
    return base::HashCombine(NodeHashes::kEmptyHash, token.high(), token.low());
  }
  return base::HashCombine(NodeHashes::kEmptyHash, token.high(), token.low(),
                           std::to_underlying(visual_data->split_layout()),
                           visual_data->split_ratio());
}

// Collects and hashes the StorageIds of all direct children of a collection.
class ChildIdProcessor : public DirectChildWalker::Processor {
 public:
  explicit ChildIdProcessor(StorageIdMapping& mapping) : mapping_(mapping) {}

  void ProcessTab(const TabInterface* tab) override {
    StorageId id = mapping_->GetStorageId(tab);
    hash_ = base::HashCombine(hash_, base::FastHash(id.AsBytes()));
  }

  void ProcessCollection(const TabCollection* collection) override {
    StorageId id = mapping_->GetStorageId(collection);
    hash_ = base::HashCombine(hash_, base::FastHash(id.AsBytes()));
  }

  size_t hash() const { return hash_; }

 private:
  const raw_ref<StorageIdMapping> mapping_;
  size_t hash_ = NodeHashes::kEmptyHash;
};

}  // namespace

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

// static
NodeHashes NodeHashCache::ComputeHashes(const TabCollection* collection,
                                        StorageIdMapping& mapping) {
  return NodeHashes{ComputePayloadHash(collection),
                    ComputeChildrenHash(collection, mapping)};
}

// static
size_t NodeHashCache::ComputePayloadHash(const TabCollection* collection) {
  CHECK(collection);
  switch (collection->type()) {
    case TabCollection::Type::GROUP:
      return HashTabGroupCollection(
          static_cast<const TabGroupTabCollection*>(collection));
    case TabCollection::Type::SPLIT:
      return HashSplitTabCollection(
          static_cast<const SplitTabCollection*>(collection));
    case TabCollection::Type::TABSTRIP:
    case TabCollection::Type::PINNED:
    case TabCollection::Type::UNPINNED:
      return NodeHashes::kEmptyHash;
  }
}

// static
size_t NodeHashCache::ComputeChildrenHash(const TabCollection* collection,
                                          StorageIdMapping& mapping) {
  CHECK(collection);
  ChildIdProcessor processor(mapping);
  DirectChildWalker walker(collection, &processor);
  walker.Walk();
  return processor.hash();
}

}  // namespace tabs
