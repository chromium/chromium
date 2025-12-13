// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_context.h"

namespace cc {

void TestLayerContext::SetVisible(bool visible) {}

base::TimeTicks TestLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    gpu::SharedImageInterface* shared_image_interface,
    const gfx::Rect& viewport_damage_rect,
    const viz::LocalSurfaceId& target_local_surface_id,
    bool frame_has_damage) {
  return base::TimeTicks::Now();
}

void TestLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    gpu::SharedImageInterface* shared_image_interface,
    bool update_damage) {}

}  // namespace cc
