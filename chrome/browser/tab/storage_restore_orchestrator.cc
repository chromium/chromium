// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_restore_orchestrator.h"

namespace tabs {

StorageRestoreOrchestrator::StorageRestoreOrchestrator(
    TabStripCollection* collection,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data)
    : collection_(collection), service_(service), loaded_data_(loaded_data) {}

StorageRestoreOrchestrator::~StorageRestoreOrchestrator() = default;

void StorageRestoreOrchestrator::OnChildrenAdded(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles) {}

void StorageRestoreOrchestrator::OnChildrenRemoved(
    const TabCollectionNodes& handles) {}

void StorageRestoreOrchestrator::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {}

void StorageRestoreOrchestrator::Save() {}

}  // namespace tabs
