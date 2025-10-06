// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_structure_tracker.h"

#include <memory>

#include "chrome/browser/tab/collection_storage_observer.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

CollectionStructureTracker::CollectionStructureTracker(
    TabStripCollection* collection,
    TabStateStorageService* service)
    : collection_(collection) {
  observer_ = std::make_unique<CollectionStorageObserver>(service);
  collection_->AddObserver(observer_.get());
}

CollectionStructureTracker::~CollectionStructureTracker() {
  collection_->RemoveObserver(observer_.get());
}

}  // namespace tabs
