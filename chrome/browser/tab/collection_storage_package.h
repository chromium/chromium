// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_COLLECTION_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_COLLECTION_STORAGE_PACKAGE_H_

#include <cstdint>
#include <vector>

#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/storage_package.h"

namespace tabs {

// A StoragePackage implementation for TabCollection data.
class CollectionStoragePackage : public StoragePackage {
 public:
  // The `metadata` represents the subtype-specific data that should be stored.
  CollectionStoragePackage(std::unique_ptr<Payload> metadata,
                           tabs_pb::Children children);
  ~CollectionStoragePackage() override;

  CollectionStoragePackage(const CollectionStoragePackage&) = delete;
  CollectionStoragePackage& operator=(const CollectionStoragePackage&) = delete;

  // StoragePackage:
  std::vector<uint8_t> SerializePayload() const override;
  std::vector<uint8_t> SerializeChildren() const override;

 private:
  std::unique_ptr<Payload> metadata_;
  tabs_pb::Children children_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_COLLECTION_STORAGE_PACKAGE_H_
