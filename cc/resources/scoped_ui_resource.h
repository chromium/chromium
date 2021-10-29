// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SCOPED_UI_RESOURCE_H_
#define CC_RESOURCES_SCOPED_UI_RESOURCE_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class UIResourceManager;

// ScopedUIResource creates an UIResource from a bitmap and a LayerTreeHost.
// This class holds a pointer to the host so that when the instance goes out of
// scope, the created resource is deleted.  On a GetBitmap call from the
// UIResource manager, ScopeUIResource always returns the reference to the
// initially given bitmap regardless of whether the request was due to lost
// resource or not.
class CC_EXPORT ScopedUIResource : public UIResourceClient {
 public:
  static std::unique_ptr<ScopedUIResource> Create(
      UIResourceManager* ui_resource_manager,
      const UIResourceBitmap& bitmap);
  ScopedUIResource(const ScopedUIResource&) = delete;
  ~ScopedUIResource() override;

  ScopedUIResource& operator=(const ScopedUIResource&) = delete;

  // UIResourceClient implementation.
  UIResourceBitmap GetBitmap(UIResourceId uid, bool resource_lost) override;
  UIResourceId id() { return id_; }

  // Returns the memory usage of the bitmap.
  size_t EstimateMemoryUsage() const { return bitmap_.SizeInBytes(); }

 protected:
  ScopedUIResource(UIResourceManager* ui_resource_manager,
                   const UIResourceBitmap& bitmap);

  UIResourceBitmap bitmap_;
  UIResourceManager* ui_resource_manager_;
  UIResourceId id_;
};

}  // namespace cc

#endif  // CC_RESOURCES_SCOPED_UI_RESOURCE_H_
