// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_observer.h"

#include "chrome/browser/tab/tab_state_storage_service.h"

namespace tabs {

CollectionStorageObserver::CollectionStorageObserver(
    tabs::TabStateStorageService* service)
    : service_(service) {}

CollectionStorageObserver::~CollectionStorageObserver() = default;

void CollectionStorageObserver::OnChildrenAdded(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {}

void CollectionStorageObserver::OnChildrenRemoved(
    const tabs::TabCollectionNodes& handles) {}

void CollectionStorageObserver::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {}

}  // namespace tabs
