// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loaded_data.h"

namespace tabs {

StorageLoadedData::StorageLoadedData() = default;
StorageLoadedData::~StorageLoadedData() = default;

StorageLoadedData::StorageLoadedData(StorageLoadedData&&) = default;
StorageLoadedData& StorageLoadedData::operator=(StorageLoadedData&&) = default;

}  // namespace tabs
