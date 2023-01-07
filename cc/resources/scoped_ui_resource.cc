// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/scoped_ui_resource.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/resources/ui_resource_manager.h"

namespace cc {

std::unique_ptr<ScopedUIResource> ScopedUIResource::Create(
    UIResourceManager* ui_resource_manager,
    const UIResourceBitmap& bitmap) {
  return base::WrapUnique(new ScopedUIResource(ui_resource_manager, bitmap));
}

ScopedUIResource::ScopedUIResource(UIResourceManager* ui_resource_manager,
                                   const UIResourceBitmap& bitmap)
    : bitmap_(bitmap), ui_resource_manager_(ui_resource_manager) {
  DCHECK(ui_resource_manager_);
  id_ = ui_resource_manager_->CreateUIResource(this);
}

// User must make sure that host is still valid before this object goes out of
// scope.
ScopedUIResource::~ScopedUIResource() {
  if (id_) {
    DCHECK(ui_resource_manager_);
    ui_resource_manager_->DeleteUIResource(id_);
  }
}

UIResourceBitmap ScopedUIResource::GetBitmap(UIResourceId uid,
                                             bool resource_lost) {
  return bitmap_;
}

}  // namespace cc
