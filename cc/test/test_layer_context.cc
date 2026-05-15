// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_context.h"

#include <vector>

#include "ui/latency/latency_info.h"

namespace cc {

void TestLayerContext::SetVisible(bool visible) {}

void TestLayerContext::SetTargetLocalSurfaceId(
    const viz::LocalSurfaceId& target_local_surface_id) {}

base::TimeTicks TestLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    gpu::SharedImageInterface* shared_image_interface,
    const gfx::Rect& viewport_damage_rect,
    bool frame_has_damage,
    bool is_flush,
    std::vector<ui::LatencyInfo> latency_info) {
  return base::TimeTicks::Now();
}

void TestLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    gpu::SharedImageInterface* shared_image_interface,
    bool update_damage) {}

}  // namespace cc
