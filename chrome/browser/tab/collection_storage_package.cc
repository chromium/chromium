// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_package.h"

#include "chrome/browser/tab/payload.h"

namespace tabs {

CollectionStoragePackage::CollectionStoragePackage(
    std::unique_ptr<Payload> metadata,
    tabs_pb::Children children)
    : metadata_(std::move(metadata)), children_(std::move(children)) {}

CollectionStoragePackage::~CollectionStoragePackage() = default;

std::string CollectionStoragePackage::SerializePayload() const {
  return metadata_->SerializePayload();
}

std::string CollectionStoragePackage::SerializeChildren() const {
  std::string payload;
  children_.SerializeToString(&payload);
  return payload;
}

}  // namespace tabs
