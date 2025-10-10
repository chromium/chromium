// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_packager.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/tab/collection_storage_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/split_collection_state.pb.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"

namespace tabs {

// Crawls the direct children of a TabCollection and adds them to the list of
// children.
class ChildProcessor : public DirectChildWalker::Processor {
 public:
  ChildProcessor(tabs_pb::Children& children_proto, StorageIdMapping& mapping)
      : children_proto_(children_proto), mapping_(mapping) {}

  void ProcessTab(const TabInterface* tab) override {
    children_proto_->add_storage_id(mapping_->GetStorageId(tab));
  }

  void ProcessCollection(const TabCollection* collection) override {
    children_proto_->add_storage_id(mapping_->GetStorageId(collection));
  }

 private:
  raw_ref<tabs_pb::Children> children_proto_;
  raw_ref<StorageIdMapping> mapping_;
};

// An empty payload of data.
class EmptyPayload : public Payload {
 public:
  EmptyPayload() = default;
  std ::string SerializePayload() const override { return ""; }
};

// A payload of data representing SplitTabCollections.
class SplitCollectionStorageData : public Payload {
 public:
  explicit SplitCollectionStorageData(
      tabs_pb::SplitCollectionState split_collection_state)
      : split_collection_state_(std::move(split_collection_state)) {}

  ~SplitCollectionStorageData() override = default;

  std ::string SerializePayload() const override {
    return split_collection_state_.SerializeAsString();
  }

 private:
  tabs_pb::SplitCollectionState split_collection_state_;
};

TabStoragePackager::TabStoragePackager() = default;
TabStoragePackager::~TabStoragePackager() = default;

std::unique_ptr<StoragePackage> TabStoragePackager::Package(
    const TabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::Children children_proto;

  ChildProcessor processor(children_proto, mapping);
  DirectChildWalker walker(collection, &processor);
  walker.Walk();

  // TODO(crbug.com/450532811): Handle collection subtype-specific data.
  std::unique_ptr<Payload> metadata;
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  if (type == TabStorageType::kSplit) {
    metadata = PackageSplitTabCollectionData(
        static_cast<const SplitTabCollection*>(collection), mapping);
  } else if (type == TabStorageType::kGroup) {
    metadata = PackageTabGroupTabCollectionData(
        static_cast<const TabGroupTabCollection*>(collection), mapping);
  } else {
    metadata = std::make_unique<EmptyPayload>();
  }

  return std::make_unique<CollectionStoragePackage>(std::move(metadata),
                                                    std::move(children_proto));
}

std::unique_ptr<Payload> TabStoragePackager::PackageSplitTabCollectionData(
    const SplitTabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::SplitCollectionState state;
  const base::Token split_tab_id = collection->GetSplitTabId().token();
  state.set_split_tab_id_high(split_tab_id.high());
  state.set_split_tab_id_low(split_tab_id.low());

  split_tabs::SplitTabData* data = collection->data();
  split_tabs::SplitTabVisualData* visual_data = data->visual_data();
  state.set_split_layout(static_cast<int32_t>(visual_data->split_layout()));
  state.set_split_ratio(visual_data->split_ratio());

  return std::make_unique<SplitCollectionStorageData>(std::move(state));
}

}  // namespace tabs
