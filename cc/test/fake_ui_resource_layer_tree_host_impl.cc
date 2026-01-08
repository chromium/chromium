// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"

#include <algorithm>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

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
  data.opaque = bitmap.GetOpaque();

  data.size = bitmap.GetSize();

  // Create a shared image of the bitmap size
  data.shared_image = gpu::ClientSharedImage::CreateForTesting(
      {viz::SinglePlaneFormat::kRGBA_8888, bitmap.GetSize(), gfx::ColorSpace(),
       GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ},
      GL_TEXTURE_2D);

  data.resource_id_for_export = resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          data.shared_image, viz::TransferableResource::ResourceSource::kTest,
          gpu::SyncToken()),
      base::DoNothing());

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

gfx::Size FakeUIResourceLayerTreeHostImpl::GetUIResourceSize(
    UIResourceId uid) const {
  auto iter = fake_ui_resource_map_.find(uid);
  if (iter != fake_ui_resource_map_.end()) {
    return iter->second.size;
  }
  return gfx::Size();
}

bool FakeUIResourceLayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid)
    const {
  auto iter = fake_ui_resource_map_.find(uid);
  CHECK(iter != fake_ui_resource_map_.end());
  return iter->second.opaque;
}

}  // namespace cc
