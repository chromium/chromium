// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_context.h"

#include "base/time/time.h"

namespace cc {

void FakeLayerContext::SetVisible(bool visible) {}

base::TimeTicks FakeLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider* context_provider,
    const gfx::Rect& viewport_damage_rect,
    const viz::LocalSurfaceId& target_local_surface_id) {
  return base::TimeTicks::Now();
}

void FakeLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider* context_provider,
    bool update_damage) {}

}  // namespace cc
