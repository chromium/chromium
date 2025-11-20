// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_packager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/tab/collection_storage_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/split_collection_state.pb.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
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
    tabs_pb::Token* token = children_proto_->add_storage_id();
    StorageIdToTokenProto(mapping_->GetStorageId(tab), token);
  }

  void ProcessCollection(const TabCollection* collection) override {
    tabs_pb::Token* token = children_proto_->add_storage_id();
    StorageIdToTokenProto(mapping_->GetStorageId(collection), token);
  }

 private:
  raw_ref<tabs_pb::Children> children_proto_;
  raw_ref<StorageIdMapping> mapping_;
};

// An empty payload of data.
class EmptyPayload : public Payload {
 public:
  EmptyPayload() = default;
  std::vector<uint8_t> SerializePayload() const override { return {}; }
};

// A payload representing the collection children.
class ChildrenPayload : public Payload {
 public:
  explicit ChildrenPayload(tabs_pb::Children children)
      : children_(std::move(children)) {}

  std::vector<uint8_t> SerializePayload() const override {
    std::vector<uint8_t> payload_vec(children_.ByteSizeLong());
    children_.SerializeToArray(payload_vec.data(), payload_vec.size());
    return payload_vec;
  }

 private:
  tabs_pb::Children children_;
};

// A payload of data representing SplitTabCollections.
class SplitCollectionStorageData : public Payload {
 public:
  explicit SplitCollectionStorageData(
      tabs_pb::SplitCollectionState split_collection_state)
      : split_collection_state_(std::move(split_collection_state)) {}

  ~SplitCollectionStorageData() override = default;

  std::vector<uint8_t> SerializePayload() const override {
    std::vector<uint8_t> payload_vec(split_collection_state_.ByteSizeLong());
    split_collection_state_.SerializeToArray(payload_vec.data(),
                                             payload_vec.size());
    return payload_vec;
  }

 private:
  tabs_pb::SplitCollectionState split_collection_state_;
};

// A payload of data representing TabGroupTabCollection.
class TabGroupCollectionStorageData : public Payload {
 public:
  explicit TabGroupCollectionStorageData(tabs_pb::TabGroupCollectionState state)
      : state_(std::move(state)) {}

  ~TabGroupCollectionStorageData() override = default;

  std::vector<uint8_t> SerializePayload() const override {
    std::vector<uint8_t> payload_vec(state_.ByteSizeLong());
    state_.SerializeToArray(payload_vec.data(), payload_vec.size());
    return payload_vec;
  }

 private:
  tabs_pb::TabGroupCollectionState state_;
};

TabStoragePackager::TabStoragePackager() = default;
TabStoragePackager::~TabStoragePackager() = default;

void PopulateChildren(tabs_pb::Children& children_proto,
                      const TabCollection* collection,
                      StorageIdMapping& mapping) {
  ChildProcessor processor(children_proto, mapping);
  DirectChildWalker walker(collection, &processor);
  walker.Walk();
}

std::unique_ptr<StoragePackage> TabStoragePackager::Package(
    const TabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::Children children_proto;
  PopulateChildren(children_proto, collection, mapping);

  return std::make_unique<CollectionStoragePackage>(
      TabStoragePackager::PackagePayload(collection, mapping),
      std::move(children_proto));
}

std::unique_ptr<Payload> TabStoragePackager::PackagePayload(
    const TabCollection* collection,
    StorageIdMapping& mapping) {
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  if (type == TabStorageType::kSplit) {
    return PackageSplitTabCollectionData(
        static_cast<const SplitTabCollection*>(collection), mapping);
  } else if (type == TabStorageType::kGroup) {
    return PackageTabGroupTabCollectionData(
        static_cast<const TabGroupTabCollection*>(collection), mapping);
  } else if (type == TabStorageType::kTabStrip) {
    return PackageTabStripCollectionData(
        static_cast<const TabStripCollection*>(collection), mapping);
  } else {
    return std::make_unique<EmptyPayload>();
  }
}

std::unique_ptr<Payload> TabStoragePackager::PackageChildren(
    const TabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::Children children_proto;

  PopulateChildren(children_proto, collection, mapping);
  return std::make_unique<ChildrenPayload>(std::move(children_proto));
}

const TabCollection* TabStoragePackager::GetRootCollection(
    const TabCollection* collection) const {
  CHECK(collection);
  const TabCollection* parent = collection->GetParentCollection();
  while (parent) {
    collection = parent;
    parent = collection->GetParentCollection();
  }
  return collection;
}

std::unique_ptr<Payload> TabStoragePackager::PackageTabGroupTabCollectionData(
    const TabGroupTabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::TabGroupCollectionState state;
  const base::Token group_id = collection->GetTabGroupId().token();
  state.set_group_id_high(group_id.high());
  state.set_group_id_low(group_id.low());

  const tab_groups::TabGroupVisualData* visual_data =
      collection->GetTabGroup()->visual_data();
  state.set_color(static_cast<int32_t>(visual_data->color()));
  state.set_is_collapsed(visual_data->is_collapsed());
  state.set_title(base::UTF16ToUTF8(visual_data->title()));

  return std::make_unique<TabGroupCollectionStorageData>(std::move(state));
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
