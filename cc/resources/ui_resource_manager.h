// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_MANAGER_H_
#define CC_RESOURCES_UI_RESOURCE_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "cc/cc_export.h"
#include "cc/resources/ui_resource_request.h"

namespace cc {
class ScopedUIResource;

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

  virtual gfx::Size GetUIResourceSize(UIResourceId id) const;

  // Methods meant to be used only internally in cc ------------

  // The current UIResourceRequestQueue is moved to the caller.
  std::vector<UIResourceRequest> TakeUIResourcesRequests();

  // Put the recreation of all UI resources into the resource queue after they
  // were evicted on the impl thread.
  void RecreateUIResources();

  // Creates a resource given an SkBitmap. Multiple calls with bitmaps that
  // share the same SkPixelRef will share a single resource ID.
  UIResourceId GetOrCreateUIResource(const SkBitmap& bitmap);

 private:
  struct UIResourceClientData {
    UIResourceClient* client;
    gfx::Size size;
  };

  std::unordered_map<UIResourceId, UIResourceClientData>
      ui_resource_client_map_;
  int next_ui_resource_id_;

  using UIResourceRequestQueue = std::vector<UIResourceRequest>;
  UIResourceRequestQueue ui_resource_request_queue_;

  // A map from bitmaps to the ScopedUIResource we've created for them. The
  // resources are never released over the duration of the lifetime of |this|.
  // If you want to release a resource added here, add a function (or extend
  // DeleteUIResource).
  std::unordered_map<SkPixelRef*, std::unique_ptr<ScopedUIResource>>
      owned_shared_resources_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_MANAGER_H_
