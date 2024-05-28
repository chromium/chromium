// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace aura {
class Window;
}  // namespace aura

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace ash {

class UiResourceManager;
class RoundedDisplayGutter;

class RoundedDisplayUiResource : public UiResource {
 public:
  RoundedDisplayUiResource();

  RoundedDisplayUiResource(const RoundedDisplayUiResource&) = delete;
  RoundedDisplayUiResource& operator=(const RoundedDisplayUiResource&) = delete;

  ~RoundedDisplayUiResource() override;
};

class ASH_EXPORT RoundedDisplayFrameFactory {
 public:
  explicit RoundedDisplayFrameFactory() = default;

  RoundedDisplayFrameFactory(const RoundedDisplayFrameFactory&) = delete;
  RoundedDisplayFrameFactory& operator=(const RoundedDisplayFrameFactory&) =
      delete;

  ~RoundedDisplayFrameFactory() = default;

  // Creates a UiResource of a given `size` and `format`. We draw the textures
  // of rounded-corners into the gpu buffer associated with the resource and
  // attach it to a compositor frame by converting it into a transferable
  // resource.
  // Note: This method is also used in unittests.
  static std::unique_ptr<RoundedDisplayUiResource> CreateUiResource(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      UiSourceId ui_source_id,
      bool is_overlay);

  // Creates and configures a compositor frame.
  // `gutters` should be in draw order i.e the gutters in the beginning are
  // drawn on top.
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      aura::Window& host_window,
      UiResourceManager& resource_manager,
      const std::vector<RoundedDisplayGutter*>& gutters);

 private:
  // Configures and appends a `TextureDrawQuad` to the `render_pass`.
  void AppendQuad(const viz::TransferableResource& resource,
                  const gfx::Transform& buffer_to_target_transform,
                  const RoundedDisplayGutter& gutter,
                  viz::CompositorRenderPass& render_pass_out) const;

  // Get a UiResource for the `gutter`. We try to reuse any existing resources
  // in `resource_manager` before creating a new resource.
  std::unique_ptr<RoundedDisplayUiResource> AcquireUiResource(
      const RoundedDisplayGutter& gutter,
      UiResourceManager& resource_manager) const;

  std::unique_ptr<RoundedDisplayUiResource> Draw(
      const RoundedDisplayGutter& gutter,
      UiResourceManager& resource_manager) const;

  // Paints the gutter's texture into the SharedImage held by `resource`.
  void Paint(const RoundedDisplayGutter& gutter,
             RoundedDisplayUiResource* resource) const;
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_FRAME_FACTORY_H_
