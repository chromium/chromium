// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/view_tree_host_root_view_frame_factory.h"

#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/ui_resource.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/display_item_list.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/paint_context.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr viz::SharedImageFormat kSharedImageFormat =
    SK_B32_SHIFT ? viz::SinglePlaneFormat::kRGBA_8888
                 : viz::SinglePlaneFormat::kBGRA_8888;

constexpr uint32_t kUiSourceId = 1u;
}  // namespace

// -----------------------------------------------------------------------------
// ViewTreeHostUiResource:

ViewTreeHostUiResource::ViewTreeHostUiResource() = default;
ViewTreeHostUiResource::~ViewTreeHostUiResource() = default;

// -----------------------------------------------------------------------------
// ViewTreeHostRootViewFrameFactory:

ViewTreeHostRootViewFrameFactory::ViewTreeHostRootViewFrameFactory(
    views::Widget* widget)
    : widget_(widget) {}

// static
std::unique_ptr<ViewTreeHostUiResource>
ViewTreeHostRootViewFrameFactory::CreateUiResource(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    UiSourceId ui_source_id,
    bool is_overlay_candidate) {
  DCHECK(!size.IsEmpty());
  DCHECK(ui_source_id > 0);

  auto resource = std::make_unique<ViewTreeHostUiResource>();
  resource->context_provider = aura::Env::GetInstance()
                                   ->context_factory()
                                   ->SharedMainThreadRasterContextProvider();
  if (!resource->context_provider) {
    LOG(ERROR) << "Failed to acquire a context provider";
    return nullptr;
  }

  gpu::SharedImageInterface* sii =
      resource->context_provider->SharedImageInterface();

  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (is_overlay_candidate) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  auto client_shared_image = sii->CreateSharedImage(
      {format, size, gfx::ColorSpace(), usage, "FastInkRootViewFrame"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
  if (!client_shared_image) {
    LOG(ERROR) << "Failed to create MappableSharedImage";
    return nullptr;
  }
  resource->SetClientSharedImage(std::move(client_shared_image));

  resource->sync_token = sii->GenVerifiedSyncToken();
  resource->damaged = true;
  resource->is_overlay_candidate = is_overlay_candidate;
  resource->format = format;
  resource->ui_source_id = ui_source_id;
  resource->resource_size = size;

  return resource;
}

std::unique_ptr<viz::CompositorFrame>
ViewTreeHostRootViewFrameFactory::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    const gfx::Rect& content_rect,
    const gfx::Rect& total_damage_rect,
    bool use_overlays,
    UiResourceManager& resource_manager) {
  auto* window = widget_->GetNativeView();
  float device_scale_factor = window->GetHost()->device_scale_factor();

  // We apply the target transform during the buffer update instead of leaving
  // it to the display compositor. The benefit is that it significantly reduces
  // the hardware overlay requirements. Frames are submitted to the compositor
  // with the inverse transform to cancel out the transformation that would
  // otherwise be done by the compositor.
  gfx::Size buffer_size =
      gfx::ScaleToCeiledSize(content_rect.size(), device_scale_factor);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  if (display.panel_rotation() == display::Display::ROTATE_90 ||
      display.panel_rotation() == display::Display::ROTATE_270) {
    buffer_size.Transpose();
  }

  // The rotation transform from the panel's original rotation to
  // the current logical rotation.
  gfx::Transform rotation_transform;
  switch (display.panel_rotation()) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      rotation_transform.Translate(buffer_size.width(), 0);
      rotation_transform.Rotate(90);
      break;
    case display::Display::ROTATE_180:
      rotation_transform.Translate(buffer_size.width(), buffer_size.height());
      rotation_transform.Rotate(180);
      break;
    case display::Display::ROTATE_270:
      rotation_transform.Translate(0, buffer_size.height());
      rotation_transform.Rotate(270);
      break;
  }

  auto resource =
      AcquireUiResource(buffer_size, use_overlays, resource_manager);

  if (!resource) {
    return nullptr;
  }

  Paint(total_damage_rect, rotation_transform, resource.get());

  if (resource->damaged) {
    DCHECK(resource->context_provider);
    gpu::SharedImageInterface* sii =
        resource->context_provider->SharedImageInterface();

    sii->UpdateSharedImage(resource->sync_token, resource->mailbox());
    resource->sync_token = sii->GenVerifiedSyncToken();
    resource->damaged = false;
  }

  viz::ResourceId frame_resource_id =
      resource_manager.OfferResource(std::move(resource));
  viz::TransferableResource transferable_resource =
      resource_manager.PrepareResourceForExport(frame_resource_id);

  auto frame = std::make_unique<viz::CompositorFrame>();
  frame->metadata.begin_frame_ack = begin_frame_ack;
  frame->metadata.begin_frame_ack.has_damage = true;
  frame->metadata.device_scale_factor = device_scale_factor;

  // TODO(crbug.com/40150287): Should this be ceil? Why do we choose floor?
  gfx::Size size_in_pixel = gfx::ToFlooredSize(
      gfx::ConvertSizeToPixels(content_rect.size(), device_scale_factor));

  gfx::Rect output_rect(size_in_pixel);
  gfx::Rect damage_rect;

  // TODO(oshima): Support partial content update.
  damage_rect = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(total_damage_rect, device_scale_factor));

  // To ensure that the damage_rect is not bigger than the output_rect. We can
  // have 1px off errors when converting from dip to pixel values for certain
  // device scale factor values what can lead the damage_rect to be bigger than
  // output_rect.
  damage_rect.Intersect(output_rect);

  auto render_pass = viz::CompositorRenderPass::Create(
      /*shared_quad_state_list_size=*/1, /*quad_list_size=*/1);

  gfx::Transform buffer_to_target_transform =
      rotation_transform.GetCheckedInverse();

  render_pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, damage_rect,
                      buffer_to_target_transform);

  AppendQuad(*render_pass, transferable_resource, output_rect, buffer_size,
             buffer_to_target_transform);

  frame->resource_list.push_back(transferable_resource);
  frame->render_pass_list.push_back(std::move(render_pass));

  return frame;
}

void ViewTreeHostRootViewFrameFactory::Paint(
    const gfx::Rect& invalidation_rect,
    const gfx::Transform& rotate_transform,
    ViewTreeHostUiResource* resource) {
  auto display_item_list = base::MakeRefCounted<cc::DisplayItemList>();
  float dsf = widget_->GetCompositor()->device_scale_factor();

  ui::PaintContext context(display_item_list.get(), dsf, invalidation_rect,
                           /*is_pixel_canvas=*/true);

  widget_->OnNativeWidgetPaint(context);
  display_item_list->Finalize();

  CHECK(resource->client_shared_image());
  auto mapping = resource->client_shared_image()->Map();
  if (!mapping) {
    TRACE_EVENT0("ui", "ViewTreeHostRootView::Paint::Map");
    LOG(ERROR) << "MapSharedImage Failed.";
    return;
  }

  SkImageInfo info = SkImageInfo::MakeN32Premul(mapping->Size().width(),
                                                mapping->Size().height());
  uint8_t* data = static_cast<uint8_t*>(mapping->Memory(0));
  int stride = mapping->Stride(0);

  auto canvas = SkCanvas::MakeRasterDirect(info, data, stride);
  canvas->setMatrix(gfx::TransformToFlattenedSkMatrix(rotate_transform));

  display_item_list->Raster(canvas.get());

  TRACE_EVENT0("ui", "ViewTreeHostRootView::Paint::Unmap");
}

void ViewTreeHostRootViewFrameFactory::AppendQuad(
    viz::CompositorRenderPass& render_pass,
    const viz::TransferableResource& resource,
    const gfx::Rect& output_rect,
    const gfx::Size& buffer_size,
    const gfx::Transform& buffer_to_target_transform) const {
  viz::SharedQuadState* quad_state =
      render_pass.CreateAndAppendSharedQuadState();

  quad_state->SetAll(buffer_to_target_transform,
                     /*layer_rect=*/output_rect,
                     /*visible_layer_rect=*/output_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/std::nullopt, /*contents_opaque=*/false,
                     /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver,
                     /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  gfx::Rect quad_rect = gfx::Rect(buffer_size);

  viz::TextureDrawQuad* texture_quad =
      render_pass.CreateAndAppendDrawQuad<viz::TextureDrawQuad>();

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

std::unique_ptr<ViewTreeHostUiResource>
ViewTreeHostRootViewFrameFactory::AcquireUiResource(
    const gfx::Size& size,
    bool is_overlay_candidate,
    UiResourceManager& resource_manager) const {
  viz::ResourceId reusable_resource_id = resource_manager.FindResourceToReuse(
      size, kSharedImageFormat, kUiSourceId);

  std::unique_ptr<ViewTreeHostUiResource> resource;

  if (reusable_resource_id != viz::kInvalidResourceId) {
    resource = base::WrapUnique(static_cast<ViewTreeHostUiResource*>(
        resource_manager.ReleaseAvailableResource(reusable_resource_id)
            .release()));
  } else {
    resource = CreateUiResource(size, kSharedImageFormat, kUiSourceId,
                                is_overlay_candidate);
  }

  return resource;
}

}  // namespace ash
