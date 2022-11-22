// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_MANAGER_H_
#define CC_RESOURCES_UI_RESOURCE_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/resources/ui_resource_request.h"

namespace cc {
class ScopedUIResource;

// UIResourceManager creates and manages UIResourceBitmaps and UIResourceIDs
// for given bitmaps for a LayerTreeHost. There are two ways to use the
// interface:
//  1. `CreateUIResource`/`DeleteUIResource` to explicitly manage the lifetime
//     of UIResources.
//  2. `GetOrCreateUIResource` to create shared UIResources backed by the same
//     SkBitmap (SkPixelRef to be exact). The created UIResources are owned by
//     the manager and are released when all references outside the manager's
//     map are dropped.
class CC_EXPORT UIResourceManager {
 public:
  UIResourceManager();
  UIResourceManager(const UIResourceManager&) = delete;
  virtual ~UIResourceManager();

  UIResourceManager& operator=(const UIResourceManager&) = delete;

  // CreateUIResource creates a resource given a bitmap.  The bitmap is
  // generated via an interface function, which is called when initializing the
  // resource and when the resource has been lost (due to lost context).  The
  // parameter of the interface is a single boolean, which indicates whether the
  // resource has been lost or not.  CreateUIResource returns an Id of the
  // resource, which is always positive.
  virtual UIResourceId CreateUIResource(UIResourceClient* client);

  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId id);

  base::flat_map<UIResourceId, gfx::Size> GetUIResourceSizes() const;

  // Methods meant to be used only internally in cc ------------

  // The current UIResourceRequestQueue is moved to the caller.
  std::vector<UIResourceRequest> TakeUIResourcesRequests();

  // Put the recreation of all UI resources into the resource queue after they
  // were evicted on the impl thread.
  void RecreateUIResources();

  // Creates a resource given a SkBitmap. Multiple calls with bitmaps that
  // share the same SkPixelRef will share a single resource ID. The returned
  // `UIResourceId` will only be valid as long as something else holds a
  // reference to the SkBitmap
  UIResourceId GetOrCreateUIResource(const SkBitmap& bitmap);

  size_t owned_shared_resources_size_for_test() const {
    return owned_shared_resources_.size();
  }

 private:
  struct UIResourceClientData {
    raw_ptr<UIResourceClient> client;
    gfx::Size size;
  };

  std::unordered_map<UIResourceId, UIResourceClientData>
      ui_resource_client_map_;
  int next_ui_resource_id_;

  using UIResourceRequestQueue = std::vector<UIResourceRequest>;
  UIResourceRequestQueue ui_resource_request_queue_;

  // A map from bitmaps to the ScopedUIResource we've created for them. The
  // resources are released when all references of SkPixelRefs outside the map
  // are dropped.
  std::unordered_map<SkPixelRef*, std::unique_ptr<ScopedUIResource>>
      owned_shared_resources_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_MANAGER_H_
