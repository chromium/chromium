// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/tab/payload.h"
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

// Adds a save children operation to the builder.
void SaveChildrenInternal(TabStateStorageUpdaterBuilder& builder,
                          const TabCollection* parent,
                          TabStateStorageService* service,
                          TabStoragePackager* packager) {
  builder.SaveChildren(service->GetStorageId(parent),
                       packager->PackageChildren(parent, *service));
}

void RemoveNodeSequence(StorageId storage_id,
                        const TabCollection* parent,
                        TabStateStorageService* service,
                        TabStoragePackager* packager,
                        TabStateStorageBackend* backend) {
  DCHECK(packager);

  TabStateStorageUpdaterBuilder builder;
  builder.RemoveNode(storage_id);

  SaveChildrenInternal(builder, parent, service, packager);
  backend->Update(builder.Build());
}

void MoveNodeSequence(const TabCollection* prev_parent,
                      const TabCollection* curr_parent,
                      TabStateStorageService* service,
                      TabStoragePackager* packager,
                      TabStateStorageBackend* backend) {
  DCHECK(packager);

  TabStateStorageUpdaterBuilder builder;
  SaveChildrenInternal(builder, prev_parent, service, packager);
  SaveChildrenInternal(builder, curr_parent, service, packager);
  backend->Update(builder.Build());
}

}  // namespace

TabStateStorageService::TabStateStorageService(
    const base::FilePath& profile_path,
    std::unique_ptr<TabStoragePackager> packager,
    TabCanonicalizer tab_canonicalizer,
    RestoreEntityTrackerFactory tracker_factory)
    : tab_backend_(profile_path),
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

void TabStateStorageService::Save(const TabInterface* tab) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package = packager_->Package(tab);
  DCHECK(package) << "Packager should return a package";

  const TabCollection* parent = tab->GetParentCollection();
  DCHECK(tab->GetParentCollection()) << "Tab must have a parent collection";
  std::string window_tag = packager_->GetWindowTag(parent);
  bool is_off_the_record = packager_->IsOffTheRecord(parent);

  StorageId storage_id = GetStorageId(tab);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record,
                   TabStorageType::kTab, std::move(package));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::Save(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package =
      packager_->Package(collection, *this);
  DCHECK(package) << "Packager should return a package";

  std::string window_tag = packager_->GetWindowTag(collection);
  bool is_off_the_record = packager_->IsOffTheRecord(collection);

  StorageId storage_id = GetStorageId(collection);
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record, type,
                   std::move(package));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::SavePayload(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<Payload> payload =
      packager_->PackagePayload(collection, *this);
  DCHECK(payload) << "Packager should return a payload";

  StorageId storage_id = GetStorageId(collection);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNodePayload(storage_id, std::move(payload));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::Remove(const TabInterface* tab,
                                    const TabCollection* prev_parent) {
  RemoveNodeSequence(GetStorageId(tab), prev_parent, this, packager_.get(),
                     &tab_backend_);
}

void TabStateStorageService::Remove(const TabCollection* collection,
                                    const TabCollection* prev_parent) {
  RemoveNodeSequence(GetStorageId(collection), prev_parent, this,
                     packager_.get(), &tab_backend_);
}

void TabStateStorageService::Move(const TabInterface* tab,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, tab->GetParentCollection(), this,
                   packager_.get(), &tab_backend_);
}

void TabStateStorageService::Move(const TabCollection* collection,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, collection->GetParentCollection(), this,
                   packager_.get(), &tab_backend_);
}

void TabStateStorageService::LoadAllNodes(const std::string& window_tag,
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
  auto builder =
      std::make_unique<StorageLoadedData::Builder>(std::move(tracker));
  tab_backend_.LoadAllNodes(window_tag, is_off_the_record, std::move(builder),
                            std::move(callback));
}

void TabStateStorageService::ClearState() {
  tab_backend_.ClearAllNodes();
}

void TabStateStorageService::ClearWindow(const std::string& window_tag) {
  tab_backend_.ClearWindow(window_tag);
}

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
