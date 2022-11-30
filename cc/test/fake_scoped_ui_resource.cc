// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scoped_ui_resource.h"

#include "base/memory/ptr_util.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

namespace {

UIResourceBitmap CreateMockUIResourceBitmap() {
  bool is_opaque = false;
  return UIResourceBitmap(gfx::Size(1, 1), is_opaque);
}

}  // anonymous namespace

std::unique_ptr<FakeScopedUIResource> FakeScopedUIResource::Create(
    UIResourceManager* ui_resource_manager) {
  return base::WrapUnique(new FakeScopedUIResource(ui_resource_manager));
}

FakeScopedUIResource::FakeScopedUIResource(
    UIResourceManager* ui_resource_manager)
    : ScopedUIResource(ui_resource_manager, CreateMockUIResourceBitmap()) {
  // The constructor of ScopedUIResource already created a resource so we need
  // to delete the created resource to wipe the state clean.
  ui_resource_manager_->DeleteUIResource(id_);
  ResetCounters();
  id_ = ui_resource_manager_->CreateUIResource(this);
}

void FakeScopedUIResource::DeleteResource() {
  ui_resource_manager_->DeleteUIResource(id_);
  id_ = 0;
}

UIResourceBitmap FakeScopedUIResource::GetBitmap(UIResourceId uid,
                                                 bool resource_lost) {
  resource_create_count++;
  if (resource_lost)
    lost_resource_count++;
  return ScopedUIResource::GetBitmap(uid, resource_lost);
}

void FakeScopedUIResource::ResetCounters() {
  resource_create_count = 0;
  lost_resource_count = 0;
}

}  // namespace cc
