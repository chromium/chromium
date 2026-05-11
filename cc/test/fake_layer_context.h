// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_CONTEXT_H_
#define CC_TEST_FAKE_LAYER_CONTEXT_H_

#include <vector>

#include "cc/trees/layer_context.h"
#include "ui/latency/latency_info.h"

namespace cc {

class FakeLayerContext : public LayerContext {
 public:
  FakeLayerContext() = default;
  ~FakeLayerContext() override = default;

  void SetVisible(bool visible) override;

  void SetTargetLocalSurfaceId(
      const viz::LocalSurfaceId& target_local_surface_id) override;

  base::TimeTicks UpdateDisplayTreeFrom(
      LayerTreeImpl& tree,
      viz::ClientResourceProvider& resource_provider,
      gpu::SharedImageInterface* shared_image_interface,
      const gfx::Rect& viewport_damage_rect,
      bool frame_has_damage,
      bool is_flush,
      std::vector<ui::LatencyInfo> latency_info,
      viz::TrackedElementRects tracked_element_rects) override;

  void UpdateDisplayTile(PictureLayerImpl& layer,
                         const Tile& tile,
                         viz::ClientResourceProvider& resource_provider,
                         gpu::SharedImageInterface* shared_image_interface,
                         bool update_damage) override;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_CONTEXT_H_
