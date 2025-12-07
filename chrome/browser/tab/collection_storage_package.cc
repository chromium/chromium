// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_package.h"

#include <string>
#include <vector>

#include "chrome/browser/tab/payload.h"

namespace tabs {

CollectionStoragePackage::CollectionStoragePackage(
    std::unique_ptr<Payload> metadata,
    tabs_pb::Children children)
    : metadata_(std::move(metadata)), children_(std::move(children)) {}

CollectionStoragePackage::~CollectionStoragePackage() = default;

std::vector<uint8_t> CollectionStoragePackage::SerializePayload() const {
  return metadata_->SerializePayload();
}

std::vector<uint8_t> CollectionStoragePackage::SerializeChildren() const {
  std::vector<uint8_t> children_vec(children_.ByteSizeLong());
  children_.SerializeToArray(children_vec.data(), children_vec.size());
  return children_vec;
}

}  // namespace tabs
