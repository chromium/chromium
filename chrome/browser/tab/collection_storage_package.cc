// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_package.h"

namespace tabs {

CollectionStoragePackage::CollectionStoragePackage() = default;
CollectionStoragePackage::~CollectionStoragePackage() = default;

std::string CollectionStoragePackage::SerializePayload() const {
  // TODO(https://crbug.com/448875689): Needs to understand type and serialize
  // data.
  return "";
}

}  // namespace tabs
