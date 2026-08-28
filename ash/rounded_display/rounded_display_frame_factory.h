// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace aura {
class Window;
}  // namespace aura

namespace gpu {
class ClientSharedImage;
class SharedImageInterface;
}  // namespace gpu

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace ash {

class RoundedDisplayGutter;

class ASH_EXPORT RoundedDisplayFrameFactory {
 public:
  explicit RoundedDisplayFrameFactory() = default;

  RoundedDisplayFrameFactory(const RoundedDisplayFrameFactory&) = delete;
  RoundedDisplayFrameFactory& operator=(const RoundedDisplayFrameFactory&) =
      delete;

  ~RoundedDisplayFrameFactory() = default;

  // Creates and configures a compositor frame.
  // `gutters` should be in draw order i.e the gutters in the beginning are
  // drawn on top.
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      aura::Window& host_window,
      viz::ClientResourceProvider& client_resource_provider,
      cc::ResourcePool& resource_pool,
      const std::vector<RoundedDisplayGutter*>& gutters);

 private:
  // Configures and appends a `TextureDrawQuad` to the `render_pass`.
  void AppendQuad(const viz::TransferableResource& resource,
                  const gfx::Transform& buffer_to_target_transform,
                  const RoundedDisplayGutter& gutter,
                  viz::CompositorRenderPass& render_pass_out) const;

  cc::ResourcePool::InUsePoolResource AcquireResource(
      const gfx::Size& size,
      bool is_overlay_candidate,
      cc::ResourcePool& resource_pool,
      gpu::SharedImageInterface* sii) const;

  // Paints the gutter's texture into the SharedImage.
  void Paint(const RoundedDisplayGutter& gutter,
             gpu::ClientSharedImage* client_shared_image) const;
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_
