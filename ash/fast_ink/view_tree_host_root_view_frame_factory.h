// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_
#define ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace gpu {
class ClientSharedImage;
class SharedImageInterface;
}  // namespace gpu

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace views {
class Widget;
}  // namespace views

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {

class ASH_EXPORT ViewTreeHostRootViewFrameFactory {
 public:
  explicit ViewTreeHostRootViewFrameFactory(views::Widget* widget);

  ViewTreeHostRootViewFrameFactory(const ViewTreeHostRootViewFrameFactory&) =
      delete;
  ViewTreeHostRootViewFrameFactory& operator=(
      const ViewTreeHostRootViewFrameFactory&) = delete;

  ~ViewTreeHostRootViewFrameFactory() = default;

  // Creates and configures a compositor frame.
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      const gfx::Rect& content_rect,
      const gfx::Rect& total_damage_rect,
      bool use_overlays,
      viz::ClientResourceProvider& client_resource_provider,
      cc::ResourcePool& resource_pool);

 private:
  void Paint(const gfx::Rect& invalidation_rect,
             const gfx::Transform& rotate_transform,
             gpu::ClientSharedImage* client_shared_image);

  // Configures and adds a `TextureDrawQuad` to the `render_pass`.
  void AppendQuad(viz::CompositorRenderPass& render_pass,
                  const viz::TransferableResource& resource,
                  const gfx::Rect& output_rect,
                  const gfx::Size& buffer_size,
                  const gfx::Transform& buffer_to_target_transform) const;

  cc::ResourcePool::InUsePoolResource AcquireResource(
      const gfx::Size& size,
      bool is_overlay_candidate,
      cc::ResourcePool& resource_pool,
      gpu::SharedImageInterface* sii) const;

  raw_ptr<views::Widget, DanglingUntriaged> widget_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_
