// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_REQUEST_H_
#define CC_RESOURCES_UI_RESOURCE_REQUEST_H_

#include <memory>

#include "base/check.h"
#include "cc/cc_export.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class CC_EXPORT UIResourceRequest {
 public:
  enum class Type {
    kCreate,
    kDelete,
  };

  UIResourceRequest(Type type, UIResourceId id);
  UIResourceRequest(Type type, UIResourceId id, const UIResourceBitmap& bitmap);
  UIResourceRequest(const UIResourceRequest& request);

  ~UIResourceRequest();

  Type GetType() const { return type_; }
  UIResourceId GetId() const { return id_; }
  UIResourceBitmap GetBitmap() const {
    DCHECK(bitmap_);
    return *bitmap_.get();
  }

  UIResourceRequest& operator=(const UIResourceRequest& request);

 private:
  Type type_;
  UIResourceId id_;
  std::unique_ptr<UIResourceBitmap> bitmap_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_REQUEST_H_
