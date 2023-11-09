// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/ui_resource_manager.h"

#include <GLES2/gl2.h>

#include <utility>

#include "ash/frame_sink/ui_resource.h"
#include "base/check.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

UiResourceManager::UiResourceManager() = default;

UiResourceManager::~UiResourceManager() {
  DCHECK(exported_resources_pool_.empty())
      << "We must reclaim all the exported resources before we can safely "
         "delete the resource manager. ";
}

viz::ResourceId UiResourceManager::FindResourceToReuse(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    UiSourceId ui_source_id) const {
  // UiResourceManager is expected to handle a few resources at a given time (
  // less than 30), therefore just using a simple linear search to find the
  // available resource.
  for (const auto& iter : available_resources_pool_) {
    const auto& resource = iter.second;

    if (resource->ui_source_id == ui_source_id &&
        resource->resource_size == size && resource->format == format) {
      return iter.first;
    }
  }

  return viz::kInvalidResourceId;
}

std::unique_ptr<UiResource> UiResourceManager::ReleaseAvailableResource(
    viz::ResourceId resource_id) {
  auto iter = available_resources_pool_.find(resource_id);

  if (iter == available_resources_pool_.end()) {
    return nullptr;
  }

  auto to_be_release_resource = std::move(iter->second);
  available_resources_pool_.erase(iter);

  return to_be_release_resource;
}

viz::ResourceId UiResourceManager::OfferResource(
    std::unique_ptr<UiResource> resource) {
  if (!resource) {
    return viz::kInvalidResourceId;
  }

  viz::ResourceId new_id = id_generator_.GenerateNextId();
  resource->resource_id = new_id;
  available_resources_pool_[new_id] = std::move(resource);

  return new_id;
}

const UiResource* UiResourceManager::PeekAvailableResource(
    viz::ResourceId resource_id) const {
  auto iter = available_resources_pool_.find(resource_id);

  if (iter != available_resources_pool_.end()) {
    return iter->second.get();
  }

  return nullptr;
}

const UiResource* UiResourceManager::PeekExportedResource(
    viz::ResourceId resource_id) const {
  auto iter = exported_resources_pool_.find(resource_id);

  if (iter != exported_resources_pool_.end()) {
    return iter->second.get();
  }

  return nullptr;
}

void UiResourceManager::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  for (const auto& entry : resources) {
    auto it = exported_resources_pool_.find(entry.id);
    DCHECK(it != exported_resources_pool_.end());

    std::unique_ptr<UiResource> resource = std::move(it->second);
    resource->sync_token = entry.sync_token;

    // Move the resource from exported pool to available resources pool.
    exported_resources_pool_.erase(it);

    if (!entry.lost) {
      available_resources_pool_[entry.id] = std::move(resource);
    }
  }
}

viz::TransferableResource UiResourceManager::PrepareResourceForExport(
    viz::ResourceId resource_id) {
  auto resource_iter = available_resources_pool_.find(resource_id);

  if (resource_iter == available_resources_pool_.end()) {
    return viz::TransferableResource();
  }

  auto to_be_exported_resource = std::move(resource_iter->second);
  available_resources_pool_.erase(resource_iter);

  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGpu(
          to_be_exported_resource->mailbox(), GL_TEXTURE_2D,
          to_be_exported_resource->sync_token,
          to_be_exported_resource->resource_size,
          to_be_exported_resource->format,
          to_be_exported_resource->is_overlay_candidate,
          viz::TransferableResource::ResourceSource::kUI);

  transferable_resource.id = resource_id;
  exported_resources_pool_[resource_id] = std::move(to_be_exported_resource);

  return transferable_resource;
}

void UiResourceManager::DamageResources() {
  for (auto& iter : exported_resources_pool_) {
    iter.second->damaged = true;
  }

  for (auto& iter : available_resources_pool_) {
    iter.second->damaged = true;
  }
}

size_t UiResourceManager::exported_resources_count() const {
  return exported_resources_pool_.size();
}

size_t UiResourceManager::available_resources_count() const {
  return available_resources_pool_.size();
}

void UiResourceManager::ClearAvailableResources() {
  available_resources_pool_.clear();
}

void UiResourceManager::LostExportedResources() {
  exported_resources_pool_.clear();
}

}  // namespace ash
