// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/token.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/payload_util.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_updater_builder.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

namespace {

template <typename T>
StorageId GetOrCreateStorageId(
    T* object,
    absl::flat_hash_map<int32_t, StorageId>& handle_map) {
  int32_t handle_id = object->GetHandle().raw_value();
  auto it = handle_map.find(handle_id);
  if (it != handle_map.end()) {
    return it->second;
  }
  StorageId storage_id = StorageId::Create();
  handle_map[handle_id] = storage_id;
  return storage_id;
}

}  // namespace

TabStateStorageService::OpenBatches::OpenBatches(
    TabStateStorageService& service,
    TabStoragePackager* packager)
    : builder(service, packager) {}

TabStateStorageService::OpenBatches::~OpenBatches() = default;

TabStateStorageService::TabStateStorageService(
    const base::FilePath& profile_path,
    bool support_off_the_record_data,
    std::unique_ptr<TabStoragePackager> packager,
    TabCanonicalizer tab_canonicalizer,
    RestoreEntityTrackerFactory tracker_factory)
    : tab_backend_(profile_path, support_off_the_record_data),
      packager_(std::move(packager)),
      tab_canonicalizer_(tab_canonicalizer),
      tracker_factory_(tracker_factory) {
  tab_backend_.Initialize();
}

TabStateStorageService::~TabStateStorageService() = default;

void TabStateStorageService::BoostPriority() {
  tab_backend_.BoostPriority();
}

StorageId TabStateStorageService::GetStorageId(
    const TabCollection* collection) {
  return ::tabs::GetOrCreateStorageId(collection,
                                      collection_handle_to_storage_id_);
}

StorageId TabStateStorageService::GetStorageId(const TabInterface* tab) {
  return ::tabs::GetOrCreateStorageId(tab_canonicalizer_.Run(tab),
                                      tab_handle_to_storage_id_);
}

void TabStateStorageService::WaitForAllPendingOperations(
    base::OnceClosure on_idle) {
  tab_backend_.WaitForAllPendingOperations(std::move(on_idle));
}

TabStateStorageService::ScopedBatch
TabStateStorageService::CreateScopedBatch() {
  if (!open_batches_) {
    open_batches_.emplace(*this, packager_.get());
  }
  open_batches_->batch_cnt++;

  return base::ScopedClosureRunner(
      base::BindOnce(&TabStateStorageService::OnScopedBatchDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageService::OnScopedBatchDestroyed() {
  if (!open_batches_) {
    return;
  }
  open_batches_->batch_cnt--;
  if (open_batches_->batch_cnt == 0) {
    CommitCurrentBatch();
  }
}

void TabStateStorageService::Save(const TabInterface* tab) {
  DCHECK(packager_);

  const TabCollection* parent = tab->GetParentCollection();
  DCHECK(tab->GetParentCollection()) << "Tab must have a parent collection";
  std::string window_tag = packager_->GetWindowTag(parent);
  bool is_off_the_record = packager_->IsOffTheRecord(parent);

  StorageId storage_id = GetStorageId(tab);

  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record,
                     TabStorageType::kTab, tab->GetHandle());
  });
}

void TabStateStorageService::Save(const TabCollection* collection) {
  DCHECK(packager_);

  std::string window_tag = packager_->GetWindowTag(collection);
  bool is_off_the_record = packager_->IsOffTheRecord(collection);

  StorageId storage_id = GetStorageId(collection);
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());

  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record, type,
                     collection->GetHandle());
  });
}

void TabStateStorageService::SavePayload(const TabCollection* collection) {
  DCHECK(packager_);

  StorageId storage_id = GetStorageId(collection);
  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.SaveNodePayload(storage_id, collection->GetHandle());
  });
}

void TabStateStorageService::SaveChildren(const TabCollection* collection) {
  DCHECK(packager_);

  StorageId storage_id = GetStorageId(collection);
  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.SaveChildren(storage_id, collection);
  });
}

void TabStateStorageService::SaveDivergentChildren(
    const TabCollection* collection,
    base::PassKey<StorageRestoreOrchestrator>) {
  DCHECK(packager_);

  StorageId storage_id = GetStorageId(collection);
  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.SaveDivergentChildren(storage_id, collection);
  });
}

void TabStateStorageService::Remove(const TabInterface* tab) {
  DCHECK(packager_);

  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.RemoveNode(GetStorageId(tab));
  });
}

void TabStateStorageService::Remove(const TabCollection* collection) {
  DCHECK(packager_);

  ApplyUpdate([&](TabStateStorageUpdaterBuilder& builder) {
    builder.RemoveNode(GetStorageId(collection));
  });
}

void TabStateStorageService::Remove(StorageId id) {
  DCHECK(packager_);

  ApplyUpdate(
      [&](TabStateStorageUpdaterBuilder& builder) { builder.RemoveNode(id); });
}

void TabStateStorageService::CommitCurrentBatch() {
  if (!open_batches_) {
    return;
  }

  tab_backend_.Update(open_batches_->builder.Build());
  open_batches_.reset();
}

void TabStateStorageService::ApplyUpdate(UpdateOperation operation) {
  if (open_batches_) {
    operation(open_batches_->builder);
  } else {
    TabStateStorageUpdaterBuilder builder(*this, packager_.get());
    operation(builder);
    tab_backend_.Update(builder.Build());
  }
}

void TabStateStorageService::CountTabsForWindow(
    std::string_view window_tag,
    bool is_off_the_record,
    CountTabsForWindowCallback callback) {
  tab_backend_.CountTabsForWindow(window_tag, is_off_the_record,
                                  std::move(callback));
}

void TabStateStorageService::LoadAllNodes(std::string_view window_tag,
                                          bool is_off_the_record,
                                          LoadDataCallback callback) {
  auto on_tab_association = base::BindRepeating(
      &TabStateStorageService::OnTabCreated, weak_ptr_factory_.GetWeakPtr());
  auto on_collection_association =
      base::BindRepeating(&TabStateStorageService::OnCollectionCreated,
                          weak_ptr_factory_.GetWeakPtr());

  // It is safe to register entities to the tracker on the background thread.
  // The callbacks bound above will only be called once the StorageLoadedData
  // has been fully constructed and passed back to the UI thread.
  std::unique_ptr<RestoreEntityTracker> tracker =
      tracker_factory_.Run(on_tab_association, on_collection_association);
  DCHECK(tracker);
  auto builder = std::make_unique<StorageLoadedData::Builder>(
      window_tag, is_off_the_record, std::move(tracker));
  tab_backend_.LoadAllNodes(window_tag, is_off_the_record, std::move(builder),
                            std::move(callback));
}

void TabStateStorageService::ClearAllWindows() {
  tab_backend_.ClearAllNodes();
}

void TabStateStorageService::ClearAllDivergenceWindows() {
  tab_backend_.ClearAllDivergentNodes();
}

void TabStateStorageService::ClearWindow(std::string_view window_tag) {
  tab_backend_.ClearWindow(window_tag);
}

void TabStateStorageService::ClearDivergenceWindow(
    std::string_view window_tag) {
  tab_backend_.ClearDivergenceWindow(window_tag);
}

void TabStateStorageService::ClearDivergentNodesForWindow(
    std::string_view window_tag,
    bool is_off_the_record) {
  tab_backend_.ClearDivergentNodesForWindow(window_tag, is_off_the_record);
}

void TabStateStorageService::ClearNodesForWindowExcept(
    std::string_view window_tag,
    bool is_off_the_record,
    std::vector<StorageId> ids) {
  tab_backend_.ClearNodesForWindowExcept(window_tag, is_off_the_record,
                                         std::move(ids));
}

void TabStateStorageService::SetKey(std::string_view window_tag,
                                    std::vector<uint8_t> key) {
  tab_backend_.SetKey(window_tag, std::move(key));
}

void TabStateStorageService::RemoveKey(std::string_view window_tag) {
  tab_backend_.RemoveKey(window_tag);
}

std::vector<uint8_t> TabStateStorageService::GenerateKey(
    std::string_view window_tag) {
  std::vector<uint8_t> key = GenerateKeyForOtrPayloads();
  tab_backend_.SetKey(window_tag, key);
  return key;
}

TabCanonicalizer TabStateStorageService::GetCanonicalizer() const {
  return tab_canonicalizer_;
}

#if defined(NDEBUG)
void TabStateStorageService::PrintAll() {
  tab_backend_.PrintAll();
}
#endif

void TabStateStorageService::OnTabCreated(StorageId storage_id,
                                          const TabInterface* tab) {
  const TabInterface* canonicalized_tab = tab_canonicalizer_.Run(tab);
  if (canonicalized_tab == nullptr) {
    // TODO(https://crbug.com/448151790): Consider removing from the database.
    // Though if a complete post-initialization raze is coming, maybe it
    // doesn't matter.
    return;
  }

  tab_handle_to_storage_id_[canonicalized_tab->GetHandle().raw_value()] =
      storage_id;
}

void TabStateStorageService::OnCollectionCreated(
    StorageId storage_id,
    const TabCollection* collection) {
  if (collection == nullptr) {
    // TODO(https://crbug.com/448151790): Consider removing from the database.
    // Though if a complete post-initialization raze is coming, maybe it
    // doesn't matter.
    return;
  }

  collection_handle_to_storage_id_[collection->GetHandle().raw_value()] =
      storage_id;
}

}  // namespace tabs
