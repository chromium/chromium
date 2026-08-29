// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_frame_factory.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "ash/frame_sink/frame_sink_utils.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {
namespace {

using RoundedCorner = RoundedDisplayGutter::RoundedCorner;

gfx::Transform GetRootRotationTransform(const aura::Window& host_window) {
  // Root transform has both the rotation and scaling of the whole UI, therefore
  // we need undo the scaling of UI to get the rotation transform.
  const auto* host = host_window.GetHost();
  gfx::Transform root_rotation_transform = host->GetRootTransform();

  float device_scale_factor = host_window.layer()->device_scale_factor();
  root_rotation_transform.Scale(1 / device_scale_factor,
                                1 / device_scale_factor);

  return root_rotation_transform;
}

viz::TextureDrawQuad::RoundedDisplayMasksInfo MapToRoundedDisplayMasksInfo(
    const std::vector<RoundedCorner>& corners) {
  DCHECK(corners.size() <= 2) << "Currently, viz can only handle textures that "
                                 "have up to 2 corner masks drawn into them";

  if (corners.size() == 1) {
    return viz::TextureDrawQuad::RoundedDisplayMasksInfo::
        CreateRoundedDisplayMasksInfo(corners.back().radius(), 0,
                                      /*is_horizontally_positioned=*/true);
  }

  std::array<const RoundedCorner*, 2> sorted_corners = {&corners.at(0),
                                                        &corners.at(1)};

  std::sort(sorted_corners.begin(), sorted_corners.end(),
            [](const RoundedCorner* c1, const RoundedCorner* c2) {
              return c1->bounds().origin() < c2->bounds().origin();
            });

  const RoundedDisplayGutter::RoundedCorner& first_corner =
      *sorted_corners.at(0);
  const RoundedDisplayGutter::RoundedCorner& second_corner =
      *sorted_corners.at(1);

  // Corners of a gutter need to be either vertically or horizontally
  // aligned.
  DCHECK(first_corner.bounds().x() == second_corner.bounds().x() ||
         first_corner.bounds().y() == second_corner.bounds().y());

  DCHECK(!first_corner.bounds().Intersects(second_corner.bounds()));

  bool is_horizontally_positioned =
      first_corner.bounds().y() == second_corner.bounds().y();

  return viz::TextureDrawQuad::RoundedDisplayMasksInfo::
      CreateRoundedDisplayMasksInfo(first_corner.radius(),
                                    second_corner.radius(),
                                    is_horizontally_positioned);
}

}  // namespace

// -----------------------------------------------------------------------------
// RoundedDisplayFrameFactory:

std::unique_ptr<viz::CompositorFrame>
RoundedDisplayFrameFactory::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    aura::Window& host_window,
    viz::ClientResourceProvider& client_resource_provider,
    cc::ResourcePool& resource_pool,
    const std::vector<RoundedDisplayGutter*>& gutters) {
  auto frame = std::make_unique<viz::CompositorFrame>();

  frame->metadata.begin_frame_ack = begin_frame_ack;
  frame->metadata.begin_frame_ack.has_damage = true;

  float device_scale_factor = host_window.layer()->device_scale_factor();
  frame->metadata.device_scale_factor = device_scale_factor;

  auto render_pass =
      viz::CompositorRenderPass::Create(/*shared_quad_state_list_size=*/1u,
                                        /*quad_list_size=*/6u);

  const display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(&host_window);

  gfx::Rect output_rect(display.GetSizeInPixel());
  render_pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, output_rect,
                      gfx::Transform());

  gfx::Transform root_rotation_inverse =
      GetRootRotationTransform(host_window).GetCheckedInverse();

  auto context_provider = frame_sink_utils::GetContextProvider();
  if (!context_provider) {
    LOG(ERROR) << "Failed to acquire a context provider";
    return nullptr;
  }

  scoped_refptr<gpu::SharedImageInterface> sii =
      context_provider->SharedImageInterface();

  for (const auto* gutter : gutters) {
    DCHECK(gutter);

    cc::ResourcePool::InUsePoolResource resource =
        frame_sink_utils::AcquirePooledResource(
            gutter->bounds().size(), gutter->NeedsOverlays(), resource_pool,
            sii.get(), "RoundedDisplayFrameUi");
    if (!resource) {
      return nullptr;
    }

    Paint(*gutter, resource.backing()->shared_image().get());

    frame_sink_utils::PrepareToExportResource(
        resource, resource_pool, client_resource_provider, sii.get(), *frame);

    // By applying the inverse of root rotation transform, we ensure that our
    // rounded corner textures are not rotated with the rest of the UI. This
    // also saves us from dealing with having the reverse rotation transform
    // requirements of using hardware overlays.
    const gfx::Transform& buffer_to_target_transform = root_rotation_inverse;

    AppendQuad(frame->resource_list.back(), buffer_to_target_transform, *gutter,
               *render_pass);

    resource_pool.ReleaseResource(std::move(resource));
  }

  frame->render_pass_list.push_back(std::move(render_pass));

  return frame;
}

void RoundedDisplayFrameFactory::Paint(
    const RoundedDisplayGutter& gutter,
    gpu::ClientSharedImage* client_shared_image) const {
  gfx::Canvas canvas(gutter.bounds().size(), 1.0, true);
  gutter.Paint(&canvas);

  CHECK(client_shared_image);
  auto mapping = client_shared_image->Map();
  if (!mapping) {
    return;
  }

  canvas.GetBitmap().readPixels(
      mapping->GetSkPixmapForPlane(
          0, SkImageInfo::MakeN32Premul(mapping->Size().width(),
                                        mapping->Size().height())),
      0, 0);
}

void RoundedDisplayFrameFactory::AppendQuad(
    const viz::TransferableResource& resource,
    const gfx::Transform& buffer_to_target_transform,
    const RoundedDisplayGutter& gutter,
    viz::CompositorRenderPass& render_pass_out) const {
  // Each gutter can be thought of as a single ui::Layer that produces only one
  // quad. Therefore the layer should be of the same size as the texture
  // produced by the gutter making layer_rect the size of the gutter in pixels.
  const gfx::Rect& layer_rect = gutter.bounds();

  viz::SharedQuadState* quad_state =
      render_pass_out.CreateAndAppendSharedQuadState();
  quad_state->SetAll(buffer_to_target_transform,
                     /*layer_rect=*/layer_rect,
                     /*visible_layer_rect=*/layer_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/std::nullopt, /*contents_opaque=*/false,
                     /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver,
                     /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  viz::TextureDrawQuad* texture_quad =
      render_pass_out.CreateAndAppendDrawQuad<viz::TextureDrawQuad>();

  // Since a single gutter is created for the full layer and we re-render the
  // full texture making the quad_rect same as the layer_rect.
  const gfx::Rect& quad_rect = layer_rect;

  // Since the gutter texture is drawn into a buffer of exact size, therefore
  // we do not need to scale uv coordinates (zoom in or out on texture) to fit
  // the buffer size.
  const auto resource_size = resource.GetSize();
  texture_quad->SetNew(
      quad_state, quad_rect, quad_rect,
      /*needs_blending=*/true, resource.id,
      /*top_left=*/gfx::PointF(0, 0),
      /*bottom_right=*/
      gfx::PointF(resource_size.width(), resource_size.height()),
      /*background=*/SkColors::kTransparent,
      /*nearest=*/false,
      /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
      /*is_tex_coords_normalized=*/false);

  texture_quad->rounded_display_masks_info =
      MapToRoundedDisplayMasksInfo(gutter.GetGutterCorners());
}

}  // namespace ash
