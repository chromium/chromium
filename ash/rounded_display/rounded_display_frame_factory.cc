// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_frame_factory.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "ash/frame_sink/ui_resource.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ipc/common/surface_handle.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
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

constexpr viz::SharedImageFormat kSharedImageFormat =
    SK_B32_SHIFT ? viz::SinglePlaneFormat::kRGBA_8888
                 : viz::SinglePlaneFormat::kBGRA_8888;

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
// RoundedDisplayUiResource:

RoundedDisplayUiResource::RoundedDisplayUiResource() = default;
RoundedDisplayUiResource::~RoundedDisplayUiResource() = default;

// -----------------------------------------------------------------------------
// RoundedDisplayFrameFactory:

// static
std::unique_ptr<RoundedDisplayUiResource>
RoundedDisplayFrameFactory::CreateUiResource(const gfx::Size& size,
                                             viz::SharedImageFormat format,
                                             UiSourceId ui_source_id,
                                             bool is_overlay) {
  DCHECK(!size.IsEmpty());
  DCHECK(ui_source_id > 0);

  auto resource = std::make_unique<RoundedDisplayUiResource>();

  if (!resource->context_provider) {
    resource->context_provider = aura::Env::GetInstance()
                                     ->context_factory()
                                     ->SharedMainThreadRasterContextProvider();
    if (!resource->context_provider) {
      LOG(ERROR) << "Failed to acquire a context provider";
      return nullptr;
    }
  }

  gpu::SharedImageInterface* sii =
      resource->context_provider->SharedImageInterface();

  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  if (is_overlay) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  auto client_shared_image = sii->CreateSharedImage({
      format, size, gfx::ColorSpace(), usage, "RoundedDisplayFrameUi"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
  if (!client_shared_image) {
    LOG(ERROR) << "Failed to create MappableSharedImage";
    return nullptr;
  }
  resource->SetClientSharedImage(std::move(client_shared_image));

  resource->sync_token = sii->GenVerifiedSyncToken();
  resource->damaged = true;
  resource->ui_source_id = ui_source_id;
  resource->is_overlay_candidate = is_overlay;
  resource->format = format;
  resource->resource_size = size;

  return resource;
}

std::unique_ptr<RoundedDisplayUiResource>
RoundedDisplayFrameFactory::AcquireUiResource(
    const RoundedDisplayGutter& gutter,
    UiResourceManager& resource_manager) const {
  gfx::Size resource_size = gutter.bounds().size();

  viz::ResourceId reusable_resource_id = resource_manager.FindResourceToReuse(
      resource_size, kSharedImageFormat, gutter.ui_source_id());

  std::unique_ptr<RoundedDisplayUiResource> resource;

  if (reusable_resource_id != viz::kInvalidResourceId) {
    resource = base::WrapUnique(static_cast<RoundedDisplayUiResource*>(
        resource_manager.ReleaseAvailableResource(reusable_resource_id)
            .release()));
  } else {
    resource = CreateUiResource(resource_size, kSharedImageFormat,
                                gutter.ui_source_id(), gutter.NeedsOverlays());
  }

  return resource;
}

std::unique_ptr<viz::CompositorFrame>
RoundedDisplayFrameFactory::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    aura::Window& host_window,
    UiResourceManager& resource_manager,
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
      display::Screen::GetScreen()->GetDisplayNearestWindow(&host_window);

  gfx::Rect output_rect(display.GetSizeInPixel());
  render_pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, output_rect,
                      gfx::Transform());

  gfx::Transform root_rotation_inverse =
      GetRootRotationTransform(host_window).GetCheckedInverse();

  for (const auto* gutter : gutters) {
    DCHECK(gutter);

    auto resource = Draw(*gutter, resource_manager);
    if (!resource) {
      return nullptr;
    }

    // By applying the inverse of root rotation transform, we ensure that our
    // rounded corner textures are not rotated with the rest of the UI. This
    // also saves us from dealing with having the reverse rotation transform
    // requirements of using hardware overlays.
    const gfx::Transform& buffer_to_target_transform = root_rotation_inverse;

    viz::ResourceId resource_id =
        resource_manager.OfferResource(std::move(resource));
    viz::TransferableResource transferable_resource =
        resource_manager.PrepareResourceForExport(resource_id);

    AppendQuad(transferable_resource, buffer_to_target_transform, *gutter,
               *render_pass);

    frame->resource_list.push_back(std::move(transferable_resource));
  }

  frame->render_pass_list.push_back(std::move(render_pass));

  return frame;
}

std::unique_ptr<RoundedDisplayUiResource> RoundedDisplayFrameFactory::Draw(
    const RoundedDisplayGutter& gutter,
    UiResourceManager& resource_manager) const {
  std::unique_ptr<RoundedDisplayUiResource> resource =
      AcquireUiResource(gutter, resource_manager);

  if (!resource) {
    return nullptr;
  }

  Paint(gutter, resource.get());

  if (resource->damaged) {
    DCHECK(resource->context_provider);
    gpu::SharedImageInterface* sii =
        resource->context_provider->SharedImageInterface();

    sii->UpdateSharedImage(resource->sync_token, resource->mailbox());

    resource->sync_token = sii->GenVerifiedSyncToken();
    resource->damaged = false;
  }

  return resource;
}

void RoundedDisplayFrameFactory::Paint(
    const RoundedDisplayGutter& gutter,
    RoundedDisplayUiResource* resource) const {
  gfx::Canvas canvas(gutter.bounds().size(), 1.0, true);
  gutter.Paint(&canvas);

  CHECK(resource->client_shared_image());
  auto mapping = resource->client_shared_image()->Map();
  if (!mapping) {
    return;
  }

  uint8_t* data = static_cast<uint8_t*>(mapping->Memory(0));
  int stride = mapping->Stride(0);

  canvas.GetBitmap().readPixels(
      SkImageInfo::MakeN32Premul(mapping->Size().width(),
                                 mapping->Size().height()),
      data, stride, 0, 0);
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
  texture_quad->SetNew(
      quad_state, quad_rect, quad_rect,
      /*needs_blending=*/true, resource.id,
      /*premultiplied=*/true, /*uv_top_left=*/gfx::PointF(0, 0),
      /*uv_bottom_right=*/gfx::PointF(1, 1),
      /*background=*/SkColors::kTransparent,
      /*flipped=*/false,
      /*nearest=*/false,
      /*secure_output=*/false, gfx::ProtectedVideoType::kClear);

  texture_quad->set_resource_size_in_pixels(resource.size);

  texture_quad->rounded_display_masks_info =
      MapToRoundedDisplayMasksInfo(gutter.GetGutterCorners());
}

}  // namespace ash
