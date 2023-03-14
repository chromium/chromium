// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_frame_factory.h"

#include <memory>
#include <vector>

#include "ash/frame_sink/ui_resource.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace ash {
namespace {

constexpr viz::ResourceFormat kResourceFormat =
    SK_B32_SHIFT ? viz::RGBA_8888 : viz::BGRA_8888;

gfx::Transform GetRootRotationTransform(const aura::Window& host_window) {
  // Root transform has both the rotation and scaling of the whole UI, therefore
  // we need undo the scaling of UI to get the rotation transform.
  auto* host = host_window.GetHost();
  gfx::Transform root_rotation_transform = host->GetRootTransform();

  float device_scale_factor = host_window.layer()->device_scale_factor();
  root_rotation_transform.Scale(1 / device_scale_factor,
                                1 / device_scale_factor);

  return root_rotation_transform;
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
                                             viz::ResourceFormat format,
                                             UiSourceId ui_source_id,
                                             bool is_overlay) {
  DCHECK(!size.IsEmpty());
  DCHECK(ui_source_id > 0);

  auto resource = std::make_unique<RoundedDisplayUiResource>();

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBuffer(size, viz::BufferFormat(kResourceFormat),
                                  gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                                  gpu::kNullSurfaceHandle, nullptr);

  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GPU memory buffer";
    return nullptr;
  }

  if (!resource->context_provider) {
    resource->context_provider = aura::Env::GetInstance()
                                     ->context_factory()
                                     ->SharedMainThreadContextProvider();
    if (!resource->context_provider) {
      LOG(ERROR) << "Failed to acquire a context provider";
      return nullptr;
    }
  }

  gpu::SharedImageInterface* sii =
      resource->context_provider->SharedImageInterface();

  uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  if (is_overlay) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  gpu::GpuMemoryBufferManager* gmb_manager =
      aura::Env::GetInstance()->context_factory()->GetGpuMemoryBufferManager();
  resource->mailbox = sii->CreateSharedImage(
      gpu_memory_buffer.get(), gmb_manager, gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage);

  resource->sync_token = sii->GenVerifiedSyncToken();
  resource->damaged = true;
  resource->ui_source_id = ui_source_id;
  resource->is_overlay_candidate = is_overlay;
  resource->format = format;
  resource->resource_size = size;
  resource->gpu_memory_buffer = std::move(gpu_memory_buffer);

  return resource;
}

std::unique_ptr<RoundedDisplayUiResource>
RoundedDisplayFrameFactory::AcquireUiResource(
    const RoundedDisplayGutter& gutter,
    UiResourceManager& resource_manager) const {
  gfx::Size resource_size = gutter.bounds().size();

  viz::ResourceId reusable_resource_id = resource_manager.FindResourceToReuse(
      resource_size, kResourceFormat, gutter.ui_source_id());

  std::unique_ptr<RoundedDisplayUiResource> resource;

  if (reusable_resource_id != viz::kInvalidResourceId) {
    resource = base::WrapUnique(static_cast<RoundedDisplayUiResource*>(
        resource_manager.ReleaseAvailableResource(reusable_resource_id)
            .release()));
  } else {
    resource = CreateUiResource(resource_size, kResourceFormat,
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

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(&host_window);

  gfx::Rect output_rect(display.GetSizeInPixel());
  render_pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, output_rect,
                      gfx::Transform());

  gfx::Transform root_rotation_inverse =
      GetRootRotationTransform(host_window).GetCheckedInverse();

  for (auto* gutter : gutters) {
    DCHECK(gutter);

    auto resource = Draw(*gutter, resource_manager);
    if (!resource) {
      return nullptr;
    }

    gfx::Transform buffer_to_target_transform;

    // Translate the gutter to correct location in the display.
    buffer_to_target_transform.Translate(gutter->bounds().x(),
                                         gutter->bounds().y());

    // By applying the inverse of root rotation transform, we ensure that our
    // rounded corner textures are not rotated with the rest of the UI. This
    // also saves us from dealing with having the reverse rotation transform
    // requirements of using hardware overlays.
    buffer_to_target_transform.PostConcat(root_rotation_inverse);

    viz::ResourceId resource_id =
        resource_manager.OfferResource(std::move(resource));
    viz::TransferableResource transferable_resource =
        resource_manager.PrepareResourceForExport(resource_id);

    AppendQuad(transferable_resource, gutter->bounds().size(),
               gutter->bounds().size(), buffer_to_target_transform,
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

  DCHECK(resource->gpu_memory_buffer);
  Paint(gutter, *resource->gpu_memory_buffer);

  if (resource->damaged) {
    DCHECK(resource->context_provider);
    gpu::SharedImageInterface* sii =
        resource->context_provider->SharedImageInterface();

    sii->UpdateSharedImage(resource->sync_token, resource->mailbox);

    resource->sync_token = sii->GenVerifiedSyncToken();
    resource->damaged = false;
  }

  return resource;
}

void RoundedDisplayFrameFactory::Paint(const RoundedDisplayGutter& gutter,
                                       gfx::GpuMemoryBuffer& buffer) const {
  gfx::Canvas canvas(gutter.bounds().size(), 1.0, true);
  gutter.Paint(&canvas);

  if (!buffer.Map()) {
    return;
  }

  uint8_t* data = static_cast<uint8_t*>(buffer.memory(0));
  int stride = buffer.stride(0);

  canvas.GetBitmap().readPixels(
      SkImageInfo::MakeN32Premul(buffer.GetSize().width(),
                                 buffer.GetSize().height()),
      data, stride, 0, 0);

  // Unmap to flush writes to buffer.
  buffer.Unmap();
}

void RoundedDisplayFrameFactory::AppendQuad(
    const viz::TransferableResource& resource,
    const gfx::Size& gutter_size,
    const gfx::Size& buffer_size,
    const gfx::Transform& buffer_to_target_transform,
    viz::CompositorRenderPass& render_pass_out) const {
  gfx::Rect output_rect(gutter_size);

  viz::SharedQuadState* quad_state =
      render_pass_out.CreateAndAppendSharedQuadState();
  quad_state->SetAll(buffer_to_target_transform,
                     /*layer_rect=*/output_rect,
                     /*visible_layer_rect=*/output_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/absl::nullopt, /*contents_opaque=*/false,
                     /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver,
                     /*sorting_context=*/0);

  gfx::Rect quad_rect(buffer_size);

  viz::TextureDrawQuad* texture_quad =
      render_pass_out.CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::RectF uv_crop(quad_rect);
  uv_crop.Scale(1.f / buffer_size.width(), 1.f / buffer_size.height());

  texture_quad->SetNew(
      quad_state, quad_rect, quad_rect,
      /*needs_blending=*/true, resource.id,
      /*premultiplied=*/true, uv_crop.origin(), uv_crop.bottom_right(),
      /*background=*/SkColors::kTransparent, vertex_opacity,
      /*flipped=*/false,
      /*nearest=*/false,
      /*secure_output=*/false, gfx::ProtectedVideoType::kClear);

  texture_quad->set_resource_size_in_pixels(resource.size);
}

}  // namespace ash
