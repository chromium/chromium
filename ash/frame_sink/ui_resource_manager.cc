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

std::unique_ptr<UiResource> UiResourceManager::GetResourceToReuse(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    UiSourceId ui_source_id) {
  // UiResourceManager is expected to handle a few resources at a given time (
  // less than 30), therefore just using a simple linear search to find the
  // available resource.
  for (auto it = available_resources_pool_.begin();
       it != available_resources_pool_.end(); ++it) {
    const auto& resource = it->second;

    if (resource->ui_source_id == ui_source_id &&
        resource->client_shared_image()->size() == size &&
        resource->client_shared_image()->format() == format) {
      auto to_be_release_resource = std::move(it->second);
      available_resources_pool_.erase(it);
      return to_be_release_resource;
    }
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

void UiResourceManager::OfferResourceForTesting(
    std::unique_ptr<UiResource> resource) {
  CHECK(resource);
  viz::ResourceId new_id = id_generator_.GenerateNextId();
  available_resources_pool_[new_id] = std::move(resource);
}

viz::TransferableResource UiResourceManager::OfferAndPrepareResourceForExport(
    std::unique_ptr<UiResource> resource) {
  CHECK(resource);

  viz::TransferableResource transferable_resource =
      viz::TransferableResource::Make(
          resource->client_shared_image(),
          viz::TransferableResource::ResourceSource::kUI, resource->sync_token,
          /*override=*/
          {
              .is_overlay_candidate = resource->is_overlay_candidate,
          });

  transferable_resource.id = id_generator_.GenerateNextId();
  exported_resources_pool_[transferable_resource.id] = std::move(resource);

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
