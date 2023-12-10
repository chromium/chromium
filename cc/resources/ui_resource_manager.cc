// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_manager.h"

#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

UIResourceManager::UIResourceManager() : next_ui_resource_id_(1) {}

UIResourceManager::~UIResourceManager() = default;

UIResourceId UIResourceManager::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(!base::Contains(ui_resource_client_map_, next_id));

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::Type::kCreate, next_id,
                            client->GetBitmap(next_id, resource_lost));
  ui_resource_request_queue_.push_back(request);

  UIResourceClientData data;
  data.client = client;
  data.size = request.GetBitmap().GetSize();

  ui_resource_client_map_[request.GetId()] = data;
  return request.GetId();
}

void UIResourceManager::DeleteUIResource(UIResourceId uid) {
  const auto iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return;

  UIResourceRequest request(UIResourceRequest::Type::kDelete, uid);
  ui_resource_request_queue_.push_back(request);
  ui_resource_client_map_.erase(iter);
}

void UIResourceManager::RecreateUIResources() {
  for (const auto& resource : ui_resource_client_map_) {
    UIResourceId uid = resource.first;
    const UIResourceClientData& data = resource.second;
    bool resource_lost = true;
    if (!base::Contains(ui_resource_request_queue_, uid,
                        &UIResourceRequest::GetId)) {
      UIResourceRequest request(UIResourceRequest::Type::kCreate, uid,
                                data.client->GetBitmap(uid, resource_lost));
      ui_resource_request_queue_.push_back(request);
    }
  }
}

base::flat_map<UIResourceId, gfx::Size> UIResourceManager::GetUIResourceSizes()
    const {
  base::flat_map<UIResourceId, gfx::Size>::container_type items(
      ui_resource_client_map_.size());
  for (const auto& pair : ui_resource_client_map_)
    items.push_back({pair.first, pair.second.size});
  return base::flat_map<UIResourceId, gfx::Size>(std::move(items));
}

std::vector<UIResourceRequest> UIResourceManager::TakeUIResourcesRequests() {
  UIResourceRequestQueue result;
  result.swap(ui_resource_request_queue_);
  return result;
}

UIResourceId UIResourceManager::GetOrCreateUIResource(const SkBitmap& bitmap) {
  DCHECK(bitmap.pixelRef()->isImmutable());
  const auto resource = owned_shared_resources_.find(bitmap.pixelRef());
  if (resource != owned_shared_resources_.end())
    return resource->second->id();

  // Evict all UIResources whose bitmaps are no longer referenced outside of the
  // map.
  std::erase_if(owned_shared_resources_,
                [](auto& pair) { return pair.second->IsUniquelyOwned(); });

  // Max capacity of `owned_shared_resources_`. A DCHECK() would fire if cache
  // size after eviction does not fall below the limit. 256 is an arbitrarily
  // chosen number that is greater than the max number of images we expect to
  // ever use concurrently.
  constexpr size_t kMaxSkBitmapResources = 256u;
  DCHECK_LT(owned_shared_resources_.size(), kMaxSkBitmapResources);

  auto scoped_resource =
      ScopedUIResource::Create(this, UIResourceBitmap(bitmap));
  auto id = scoped_resource->id();
  owned_shared_resources_[bitmap.pixelRef()] = std::move(scoped_resource);
  return id;
}

}  // namespace cc
