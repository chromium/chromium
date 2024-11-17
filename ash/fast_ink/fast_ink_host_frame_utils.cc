// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host_frame_utils.h"

#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/ui_resource.h"
#include "base/check.h"
#include "base/logging.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {
namespace fast_ink_internal {
namespace {

// Get a UiResource to paint the texture. We try to reuse any
// existing resources in `resource_manager` before creating a new resource.
std::unique_ptr<UiResource> AcquireUiResource(
    const gfx::Size& size,
    bool is_overlay_candidate,
    UiResourceManager* resource_manager,
    gpu::Mailbox mailbox,
    gpu::SyncToken sync_token) {
  CHECK(!mailbox.IsZero());
  viz::ResourceId reusable_resource_id = resource_manager->FindResourceToReuse(
      size, kFastInkSharedImageFormat, kFastInkUiSourceId);
  std::unique_ptr<UiResource> resource;
  if (reusable_resource_id != viz::kInvalidResourceId) {
    resource = resource_manager->ReleaseAvailableResource(reusable_resource_id);
    CHECK(mailbox == resource->mailbox());
  } else {
    resource = CreateUiResource(size, kFastInkUiSourceId, is_overlay_candidate,
                                mailbox, sync_token);
  }

  return resource;
}

// Configures and adds a `TextureDrawQuad` to the `render_pass_out`.
void AppendQuad(const viz::TransferableResource& resource,
                const gfx::Rect& output_rect,
                const gfx::Rect& quad_rect,
                const gfx::Size& buffer_size,
                const gfx::Transform& buffer_to_target_transform,
                viz::CompositorRenderPass& render_pass_out) {
  viz::SharedQuadState* quad_state =
      render_pass_out.CreateAndAppendSharedQuadState();

  quad_state->SetAll(buffer_to_target_transform,
                     /*layer_rect=*/output_rect,
                     /*visible_layer_rect=*/output_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/std::nullopt, /*contents_opaque=*/false,
                     /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver,
                     /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  viz::TextureDrawQuad* texture_quad =
      render_pass_out.CreateAndAppendDrawQuad<viz::TextureDrawQuad>();

  gfx::RectF uv_crop(quad_rect);
  uv_crop.Scale(1.f / buffer_size.width(), 1.f / buffer_size.height());

  texture_quad->SetNew(quad_state, quad_rect, quad_rect,
                       /*needs_blending=*/true, resource.id,
                       /*premultiplied=*/true, uv_crop.origin(),
                       uv_crop.bottom_right(), SkColors::kTransparent,
                       /*flipped=*/false,
                       /*nearest=*/false,
                       /*secure_output=*/false,
                       gfx::ProtectedVideoType::kClear);

  texture_quad->set_resource_size_in_pixels(resource.size);
}

}  // namespace

gfx::Rect BufferRectFromWindowRect(
    const gfx::Transform& window_to_buffer_transform,
    const gfx::Size& buffer_size,
    const gfx::Rect& window_rect) {
  gfx::Rect buffer_rect = cc::MathUtil::MapEnclosingClippedRect(
      window_to_buffer_transform, window_rect);
  // Buffer rect is not bigger than actual buffer.
  buffer_rect.Intersect(gfx::Rect(buffer_size));
  return buffer_rect;
}

scoped_refptr<gpu::ClientSharedImage> CreateMappableSharedImage(
    const gfx::Size& size,
    gpu::SharedImageUsageSet shared_image_usage,
    gfx::BufferUsage buffer_usage) {
  return GetContextProvider()->SharedImageInterface()->CreateSharedImage(
      {kFastInkSharedImageFormat, size, gfx::ColorSpace(), shared_image_usage,
       "FastInkHostUIResource"},
      gpu::kNullSurfaceHandle, buffer_usage);
}

std::unique_ptr<UiResource> CreateUiResource(
    const gfx::Size& size,
    UiSourceId ui_source_id,
    bool is_overlay_candidate,
    gpu::Mailbox mailbox,
    gpu::SyncToken sync_token) {
  DCHECK(!size.IsEmpty());
  DCHECK(ui_source_id > 0);
  CHECK(!mailbox.IsZero());

  auto resource = std::make_unique<UiResource>();

  resource->context_provider = GetContextProvider();

  if (!resource->context_provider) {
    LOG(ERROR) << "Failed to acquire a context provider";
    return nullptr;
  }

  // This UiResource is operating on a shared SharedImage.
  resource->SetExternallyOwnedMailbox(mailbox);
  resource->sync_token = sync_token;

  resource->damaged = true;
  resource->is_overlay_candidate = is_overlay_candidate;
  resource->format = kFastInkSharedImageFormat;
  resource->ui_source_id = ui_source_id;
  resource->resource_size = size;
  return resource;
}

std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    const gfx::Rect& content_rect,
    const gfx::Rect& total_damage_rect,
    bool auto_update,
    const aura::Window& host_window,
    const gfx::Size& buffer_size,
    UiResourceManager* resource_manager,
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    gpu::SyncToken sync_token) {
  float device_scale_factor = host_window.layer()->device_scale_factor();
  const gfx::Transform& window_to_buffer_transform =
      host_window.GetHost()->GetRootTransform();
  gfx::Size window_size_in_dip = host_window.GetBoundsInScreen().size();

  const gfx::Size window_size_in_pixel =
      gfx::ScaleToEnclosingRectIgnoringError(gfx::Rect{window_size_in_dip},
                                             device_scale_factor)
          .size();

  // NOTE: `shared_image` is guaranteed to be non-null by contract of this
  // method, and ClientSharedImage::mailbox() is guaranteed to be non-zero by
  // contract of *that* method.
  CHECK(shared_image);
  auto mailbox = shared_image->mailbox();

  // In auto_update mode, we use hardware overlays to render the content.
  auto resource = AcquireUiResource(buffer_size, auto_update, resource_manager,
                                    mailbox, sync_token);

  if (!resource) {
    return nullptr;
  }

  if (resource->damaged) {
    DCHECK(resource->context_provider);
    gpu::SharedImageInterface* sii =
        resource->context_provider->SharedImageInterface();

    sii->UpdateSharedImage(resource->sync_token, resource->mailbox());
    resource->sync_token = sii->GenVerifiedSyncToken();
    resource->damaged = false;
  }

  viz::ResourceId frame_resource_id =
      resource_manager->OfferResource(std::move(resource));
  viz::TransferableResource transferable_resource =
      resource_manager->PrepareResourceForExport(frame_resource_id);

  gfx::Transform target_to_buffer_transform(window_to_buffer_transform);
  target_to_buffer_transform.Scale(1.f / device_scale_factor,
                                   1.f / device_scale_factor);

  gfx::Transform buffer_to_target_transform =
      target_to_buffer_transform.GetCheckedInverse();

  const gfx::Rect output_rect(window_size_in_pixel);

  gfx::Rect quad_rect;
  gfx::Rect damage_rect;

  // Continuously redraw the full output rectangle when in auto-update mode.
  // This is necessary in order to allow single buffered updates without having
  // buffer changes outside the contents area cause artifacts.
  if (auto_update) {
    quad_rect = gfx::Rect(buffer_size);
    damage_rect = gfx::Rect(output_rect);
  } else {
    // Use minimal quad and damage rectangles when auto-refresh mode is off.
    quad_rect = BufferRectFromWindowRect(window_to_buffer_transform,
                                         buffer_size, content_rect);
    damage_rect = gfx::ToEnclosingRect(
        gfx::ConvertRectToPixels(total_damage_rect, device_scale_factor));

    // To ensure that the damage_rect is not bigger than the output_rect. We can
    // have 1px off errors when converting from dip to pixel values for certain
    // device scale factor values what can lead the damage_rect to be bigger
    // than output_rect.
    damage_rect.Intersect(output_rect);
  }

  auto frame = std::make_unique<viz::CompositorFrame>();
  frame->metadata.begin_frame_ack = begin_frame_ack;
  frame->metadata.begin_frame_ack.has_damage = true;
  frame->metadata.device_scale_factor = device_scale_factor;

  auto render_pass = viz::CompositorRenderPass::Create(
      /*shared_quad_state_list_size=*/1, /*quad_list_size=*/1);

  render_pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, damage_rect,
                      buffer_to_target_transform);

  AppendQuad(transferable_resource, output_rect, quad_rect, buffer_size,
             buffer_to_target_transform, *render_pass);

  frame->resource_list.push_back(std::move(transferable_resource));
  frame->render_pass_list.push_back(std::move(render_pass));

  return frame;
}

scoped_refptr<viz::RasterContextProvider> GetContextProvider() {
  return aura::Env::GetInstance()
      ->context_factory()
      ->SharedMainThreadRasterContextProvider();
}

}  // namespace fast_ink_internal
}  // namespace ash
