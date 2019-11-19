// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"

#include "base/bind_helpers.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/test/fake_layer_tree_host_impl.h"

namespace cc {

FakeUIResourceLayerTreeHostImpl::FakeUIResourceLayerTreeHostImpl(
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner)
    : FakeLayerTreeHostImpl(task_runner_provider, task_graph_runner) {}

FakeUIResourceLayerTreeHostImpl::~FakeUIResourceLayerTreeHostImpl() = default;

void FakeUIResourceLayerTreeHostImpl::CreateUIResource(
    UIResourceId uid,
    const UIResourceBitmap& bitmap) {
  if (ResourceIdForUIResource(uid))
    DeleteUIResource(uid);

  UIResourceData data;

  data.resource_id_for_export = resource_provider()->ImportResource(
      viz::TransferableResource::MakeGL(
          gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
          bitmap.GetSize(), false /* is_overlay_candidate */),
      viz::SingleReleaseCallback::Create(base::DoNothing()));

  data.opaque = bitmap.GetOpaque();
  fake_ui_resource_map_[uid] = std::move(data);
}

void FakeUIResourceLayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  viz::ResourceId id = ResourceIdForUIResource(uid);
  if (id) {
    resource_provider()->RemoveImportedResource(id);
    fake_ui_resource_map_.erase(uid);
  }
}

viz::ResourceId FakeUIResourceLayerTreeHostImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  auto iter = fake_ui_resource_map_.find(uid);
  if (iter != fake_ui_resource_map_.end())
    return iter->second.resource_id_for_export;
  return viz::kInvalidResourceId;
}

bool FakeUIResourceLayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid)
    const {
  auto iter = fake_ui_resource_map_.find(uid);
  DCHECK(iter != fake_ui_resource_map_.end());
  return iter->second.opaque;
}

}  // namespace cc
