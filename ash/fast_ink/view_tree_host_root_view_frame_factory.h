// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_
#define ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/compositor_frame.h"

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

class UiResourceManager;

class ViewTreeHostUiResource : public UiResource {
 public:
  ViewTreeHostUiResource();

  ViewTreeHostUiResource(const ViewTreeHostUiResource&) = delete;
  ViewTreeHostUiResource& operator=(const ViewTreeHostUiResource&) = delete;

  ~ViewTreeHostUiResource() override;
};

class ASH_EXPORT ViewTreeHostRootViewFrameFactory {
 public:
  explicit ViewTreeHostRootViewFrameFactory(views::Widget* widget);

  ViewTreeHostRootViewFrameFactory(const ViewTreeHostRootViewFrameFactory&) =
      delete;
  ViewTreeHostRootViewFrameFactory& operator=(
      const ViewTreeHostRootViewFrameFactory&) = delete;

  ~ViewTreeHostRootViewFrameFactory() = default;

  // Creates a ViewTreeHostUiResource of a given `size` and `format`. We draw
  // the textures of view tree host by `widget` into the gpu buffer associated
  // with the resource and attach it to a compositor frame by converting it into
  // a transferable resource. Note: This method is also used in unittests.
  static std::unique_ptr<ViewTreeHostUiResource> CreateUiResource(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      UiSourceId ui_source_id,
      bool is_overlay_candidate);

  // Creates and configures a compositor frame.
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      const gfx::Rect& content_rect,
      const gfx::Rect& total_damage_rect,
      bool use_overlays,
      UiResourceManager& resource_manager);

 private:
  void Paint(const gfx::Rect& invalidation_rect,
             const gfx::Transform& rotate_transform,
             ViewTreeHostUiResource* resource);

  // Configures and adds a `TextureDrawQuad` to the `render_pass`.
  void AppendQuad(viz::CompositorRenderPass& render_pass,
                  const viz::TransferableResource& resource,
                  const gfx::Rect& output_rect,
                  const gfx::Size& buffer_size,
                  const gfx::Transform& buffer_to_target_transform) const;

  // Get a ViewTreeHostUiResource to paint the texture. We try to reuse any
  // existing resources in `resource_manager` before creating a new resource.
  std::unique_ptr<ViewTreeHostUiResource> AcquireUiResource(
      const gfx::Size& size,
      bool is_overlay_candidate,
      UiResourceManager& resource_manager) const;

  raw_ptr<views::Widget, DanglingUntriaged> widget_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_FRAME_FACTORY_H_
