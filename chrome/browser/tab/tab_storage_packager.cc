// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_packager.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/tab/collection_storage_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "components/tabs/public/direct_child_walker.h"

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

TabStoragePackager::TabStoragePackager() = default;
TabStoragePackager::~TabStoragePackager() = default;

std::unique_ptr<StoragePackage> TabStoragePackager::Package(
    const TabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::Children children_proto;

  ChildProcessor processor(children_proto, mapping);
  DirectChildWalker walker(collection, &processor);
  walker.Walk();

  // TODO(https://crbug.com/448875689): Fill this package with collection
  // specific data.
  return std::make_unique<CollectionStoragePackage>(
      std::make_unique<EmptyPayload>(), std::move(children_proto));
}

}  // namespace tabs
