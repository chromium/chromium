// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_CONTEXT_H_
#define CC_TEST_FAKE_LAYER_CONTEXT_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/trees/layer_context.h"
#include "ui/latency/latency_info.h"

namespace cc {

class FakeLayerContext : public LayerContext {
 public:
  // What UpdateDisplayTreeFrom() received, for tests to inspect.
  struct Updates {
    size_t count = 0;
    std::optional<viz::FrameIntervalInputs> last_frame_interval_inputs;
  };

  FakeLayerContext() = default;
  // Records updates into `updates`, which must outlive this context.
  explicit FakeLayerContext(Updates* updates) : updates_(updates) {}
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
      viz::TrackedElementRects tracked_element_rects,
      std::optional<viz::FrameIntervalInputs> frame_interval_inputs) override;

  void UpdateDisplayTile(PictureLayerImpl& layer,
                         const Tile& tile,
                         viz::ClientResourceProvider& resource_provider,
                         gpu::SharedImageInterface* shared_image_interface,
                         bool update_damage) override;

 private:
  raw_ptr<Updates> updates_ = nullptr;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_CONTEXT_H_
