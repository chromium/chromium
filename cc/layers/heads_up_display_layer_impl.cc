// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/debug/debug_colors.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/memory_history.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/trace_util.h"

namespace cc {

namespace {

void DrawArc(PaintCanvas* canvas,
             const SkRect& oval,
             SkScalar start_angle,
             SkScalar sweep_angle,
             const PaintFlags& flags) {
  DCHECK_GT(sweep_angle, 0.f);
  DCHECK_LT(sweep_angle, 360.f);
  SkPath path;
  path.moveTo(oval.centerX(), oval.centerY());
  path.arcTo(oval, start_angle, sweep_angle, false /* forceMoveTo */);
  path.close();
  canvas->drawPath(path, flags);
}

class DummyImageProvider : public ImageProvider {
 public:
  DummyImageProvider() = default;
  ~DummyImageProvider() override = default;
  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override {
    NOTREACHED();
    return ScopedResult();
  }
};

}  // namespace

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
    : LayerImpl(tree_impl, id) {}

HeadsUpDisplayLayerImpl::~HeadsUpDisplayLayerImpl() {
  ReleaseResources();
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

class HudGpuBacking : public ResourcePool::GpuBacking {
 public:
  ~HudGpuBacking() override {
    if (mailbox.IsZero())
      return;
    if (returned_sync_token.HasData())
      shared_image_interface->DestroySharedImage(returned_sync_token, mailbox);
    else if (mailbox_sync_token.HasData())
      shared_image_interface->DestroySharedImage(mailbox_sync_token, mailbox);
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
    if (mailbox.IsZero())
      return;

    auto tracing_guid = gpu::GetSharedImageGUIDForTracing(mailbox);
    pmd->CreateSharedGlobalAllocatorDump(tracing_guid);
    pmd->AddOwnershipEdge(buffer_dump_guid, tracing_guid, importance);
  }

  gpu::SharedImageInterface* shared_image_interface = nullptr;
};

class HudSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~HudSoftwareBacking() override {
    layer_tree_frame_sink->DidDeleteSharedBitmap(shared_bitmap_id);
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
    pmd->CreateSharedMemoryOwnershipEdge(buffer_dump_guid,
                                         shared_mapping.guid(), importance);
  }

  LayerTreeFrameSink* layer_tree_frame_sink;
  base::WritableSharedMemoryMapping shared_mapping;
};

bool HeadsUpDisplayLayerImpl::WillDraw(
    DrawMode draw_mode,
    viz::ClientResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE &&
      !LayerImpl::WillDraw(draw_mode, resource_provider)) {
    return false;
  }

  int max_texture_size = layer_tree_impl()->max_texture_size();
  internal_contents_scale_ = GetIdealContentsScale();
  internal_content_bounds_ =
      gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_);
  internal_content_bounds_.SetToMin(
      gfx::Size(max_texture_size, max_texture_size));

  return true;
}

void HeadsUpDisplayLayerImpl::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state, internal_contents_scale_,
                                contents_opaque());

  // Appends a dummy quad here, which will be updated later once the resource
  // is ready in UpdateHudTexture(). We don't add a TextureDrawQuad directly
  // because we don't have a ResourceId for it yet, and ValidateQuadResources()
  // would fail. UpdateHudTexture() happens after all quads are appended for all
  // layers.
  gfx::Rect quad_rect(internal_content_bounds_);
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, quad_rect, SK_ColorTRANSPARENT,
               false);
  ValidateQuadResources(quad);
  current_quad_ = quad;
}

void HeadsUpDisplayLayerImpl::UpdateHudTexture(
    DrawMode draw_mode,
    LayerTreeFrameSink* layer_tree_frame_sink,
    viz::ClientResourceProvider* resource_provider,
    bool gpu_raster,
    const viz::CompositorRenderPassList& list) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return;

  // Update state that will be drawn.
  UpdateHudContents();

  // TODO(penghuang): Do not use worker_context_provider() when context_provider
  // is changed to RasterContextProvider.
  // https://crbug.com/c/1286950
  auto* raster_context_provider =
      gpu_raster ? layer_tree_frame_sink->worker_context_provider() : nullptr;
  base::Optional<viz::RasterContextProvider::ScopedRasterContextLock> lock;
  bool use_oopr = false;
  if (raster_context_provider) {
    lock.emplace(raster_context_provider);
    use_oopr =
        raster_context_provider->ContextCapabilities().supports_oop_raster;
    if (!use_oopr) {
      raster_context_provider = nullptr;
      lock.reset();
    }
  }

  auto* context_provider = layer_tree_frame_sink->context_provider();
  if (!pool_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        layer_tree_impl()->task_runner_provider()->HasImplThread()
            ? layer_tree_impl()->task_runner_provider()->ImplThreadTaskRunner()
            : layer_tree_impl()->task_runner_provider()->MainThreadTaskRunner();
    pool_ = std::make_unique<ResourcePool>(
        resource_provider, context_provider, std::move(task_runner),
        ResourcePool::kDefaultExpirationDelay,
        layer_tree_impl()->settings().disallow_non_exact_resource_reuse);
  }

  // Return ownership of the previous frame's resource to the pool, so we
  // can reuse it once it is not busy in the display compositor. This is safe to
  // do here because the previous frame has been shipped to the display
  // compositor by the time we UpdateHudTexture for the current frame.
  if (in_flight_resource_)
    pool_->ReleaseResource(std::move(in_flight_resource_));

  // Allocate a backing for the resource if needed, either for gpu or software
  // compositing.
  ResourcePool::InUsePoolResource pool_resource;
  if (draw_mode == DRAW_MODE_HARDWARE) {
    DCHECK(raster_context_provider || context_provider);
    const auto& caps = raster_context_provider
                           ? raster_context_provider->ContextCapabilities()
                           : context_provider->ContextCapabilities();
    viz::ResourceFormat format =
        gpu_raster ? viz::PlatformColor::BestSupportedRenderBufferFormat(caps)
                   : viz::PlatformColor::BestSupportedTextureFormat(caps);
    pool_resource = pool_->AcquireResource(internal_content_bounds_, format,
                                           gfx::ColorSpace());

    if (!pool_resource.gpu_backing()) {
      auto backing = std::make_unique<HudGpuBacking>();
      auto* sii = raster_context_provider
                      ? raster_context_provider->SharedImageInterface()
                      : context_provider->SharedImageInterface();
      backing->shared_image_interface = sii;
      backing->InitOverlayCandidateAndTextureTarget(
          pool_resource.format(), caps,
          layer_tree_impl()
              ->settings()
              .resource_settings.use_gpu_memory_buffer_resources);

      uint32_t flags = gpu::SHARED_IMAGE_USAGE_DISPLAY;
      if (use_oopr) {
        flags |= gpu::SHARED_IMAGE_USAGE_RASTER |
                 gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      } else if (gpu_raster) {
        flags |= gpu::SHARED_IMAGE_USAGE_GLES2 |
                 gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
      }
      if (backing->overlay_candidate)
        flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      backing->mailbox = sii->CreateSharedImage(
          pool_resource.format(), pool_resource.size(),
          pool_resource.color_space(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType, flags, gpu::kNullSurfaceHandle);
      if (raster_context_provider) {
        auto* ri = raster_context_provider->RasterInterface();
        ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
      } else {
        auto* gl = context_provider->ContextGL();
        gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
      }
      pool_resource.set_gpu_backing(std::move(backing));
    } else if (pool_resource.gpu_backing()->returned_sync_token.HasData()) {
      if (raster_context_provider) {
        auto* ri = raster_context_provider->RasterInterface();
        ri->WaitSyncTokenCHROMIUM(
            pool_resource.gpu_backing()->returned_sync_token.GetConstData());
      } else {
        auto* gl = context_provider->ContextGL();
        gl->WaitSyncTokenCHROMIUM(
            pool_resource.gpu_backing()->returned_sync_token.GetConstData());
      }
      pool_resource.gpu_backing()->returned_sync_token = gpu::SyncToken();
    }
  } else {
    DCHECK_EQ(draw_mode, DRAW_MODE_SOFTWARE);

    pool_resource = pool_->AcquireResource(internal_content_bounds_,
                                           viz::RGBA_8888, gfx::ColorSpace());

    if (!pool_resource.software_backing()) {
      auto backing = std::make_unique<HudSoftwareBacking>();
      backing->layer_tree_frame_sink = layer_tree_frame_sink;
      backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
      base::MappedReadOnlyRegion shm =
          viz::bitmap_allocation::AllocateSharedBitmap(pool_resource.size(),
                                                       pool_resource.format());
      backing->shared_mapping = std::move(shm.mapping);

      layer_tree_frame_sink->DidAllocateSharedBitmap(std::move(shm.region),
                                                     backing->shared_bitmap_id);

      pool_resource.set_software_backing(std::move(backing));
    }
  }

  if (gpu_raster) {
    // If using |gpu_raster| we DrawHudContents() directly to a gpu texture,
    // which is wrapped in an SkSurface.
    DCHECK_EQ(draw_mode, DRAW_MODE_HARDWARE);
    DCHECK(pool_resource.gpu_backing());
    auto* backing = static_cast<HudGpuBacking*>(pool_resource.gpu_backing());

    if (use_oopr) {
      const auto& size = pool_resource.size();
      auto display_item_list = base::MakeRefCounted<DisplayItemList>(
          DisplayItemList::kTopLevelDisplayItemList);
      RecordPaintCanvas canvas(display_item_list.get(),
                               SkRect::MakeIWH(size.width(), size.height()));
      display_item_list->StartPaint();
      DrawHudContents(&canvas);
      display_item_list->EndPaintOfUnpaired(gfx::Rect(size));
      display_item_list->Finalize();

      auto* ri = raster_context_provider->RasterInterface();
      constexpr GLuint background_color = SkColorSetARGB(0, 0, 0, 0);
      constexpr GLuint msaa_sample_count = -1;
      constexpr bool can_use_lcd_text = true;
      ri->BeginRasterCHROMIUM(background_color, msaa_sample_count,
                              can_use_lcd_text, gfx::ColorSpace::CreateSRGB(),
                              backing->mailbox.name);
      gfx::Vector2dF post_translate(0.f, 0.f);
      DummyImageProvider image_provider;
      size_t max_op_size_limit =
          gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
      ri->RasterCHROMIUM(display_item_list.get(), &image_provider, size,
                         gfx::Rect(size), gfx::Rect(size), post_translate,
                         1.f /* post_scale */, false /* requires_clear */,
                         &max_op_size_limit);
      ri->EndRasterCHROMIUM();
      backing->mailbox_sync_token =
          viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
    } else {
      auto* gl = context_provider->ContextGL();
      GLuint mailbox_texture_id =
          gl->CreateAndTexStorage2DSharedImageCHROMIUM(backing->mailbox.name);
      gl->BeginSharedImageAccessDirectCHROMIUM(
          mailbox_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

      {
        ScopedGpuRaster scoped_gpu_raster(context_provider);
        viz::ClientResourceProvider::ScopedSkSurface scoped_surface(
            context_provider->GrContext(),
            pool_resource.color_space().ToSkColorSpace(), mailbox_texture_id,
            backing->texture_target, pool_resource.size(),
            pool_resource.format(), false /* can_use_lcd_text */,
            0 /* msaa_sample_count */);
        SkSurface* surface = scoped_surface.surface();
        if (!surface) {
          pool_->ReleaseResource(std::move(pool_resource));
          return;
        }
        SkiaPaintCanvas canvas(surface->getCanvas());
        DrawHudContents(&canvas);
      }

      gl->EndSharedImageAccessDirectCHROMIUM(mailbox_texture_id);
      gl->DeleteTextures(1, &mailbox_texture_id);
      backing->mailbox_sync_token =
          viz::ClientResourceProvider::GenerateSyncTokenHelper(gl);
    }
  } else if (draw_mode == DRAW_MODE_HARDWARE) {
    // If not using |gpu_raster| but using gpu compositing, we DrawHudContents()
    // into a software bitmap and upload it to a texture for compositing.
    DCHECK(pool_resource.gpu_backing());
    auto* backing = static_cast<HudGpuBacking*>(pool_resource.gpu_backing());
    gpu::gles2::GLES2Interface* gl =
        layer_tree_impl()->context_provider()->ContextGL();

    if (!staging_surface_ ||
        gfx::SkISizeToSize(staging_surface_->getCanvas()->getBaseLayerSize()) !=
            pool_resource.size()) {
      staging_surface_ = SkSurface::MakeRasterN32Premul(
          pool_resource.size().width(), pool_resource.size().height());
    }

    SkiaPaintCanvas canvas(staging_surface_->getCanvas());
    DrawHudContents(&canvas);

    TRACE_EVENT0("cc", "UploadHudTexture");
    SkPixmap pixmap;
    staging_surface_->peekPixels(&pixmap);

    GLuint mailbox_texture_id =
        gl->CreateAndTexStorage2DSharedImageCHROMIUM(backing->mailbox.name);
    gl->BeginSharedImageAccessDirectCHROMIUM(
        mailbox_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

    gl->BindTexture(backing->texture_target, mailbox_texture_id);
    DCHECK(GLSupportsFormat(pool_resource.format()));
    // We should use gl compatible format for skia SW rasterization.
    constexpr GLenum format = SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT;
    constexpr GLenum type = GL_UNSIGNED_BYTE;
    gl->TexSubImage2D(
        backing->texture_target, 0, 0, 0, pool_resource.size().width(),
        pool_resource.size().height(), format, type, pixmap.addr());

    gl->EndSharedImageAccessDirectCHROMIUM(mailbox_texture_id);
    gl->DeleteTextures(1, &mailbox_texture_id);
    backing->mailbox_sync_token =
        viz::ClientResourceProvider::GenerateSyncTokenHelper(gl);
  } else {
    // If not using gpu compositing, we DrawHudContents() directly into a shared
    // memory bitmap, wrapped in an SkSurface, that can be shared to the display
    // compositor.
    DCHECK_EQ(draw_mode, DRAW_MODE_SOFTWARE);
    DCHECK(pool_resource.software_backing());

    SkImageInfo info = SkImageInfo::MakeN32Premul(
        pool_resource.size().width(), pool_resource.size().height());
    auto* backing =
        static_cast<HudSoftwareBacking*>(pool_resource.software_backing());
    sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(
        info, backing->shared_mapping.memory(), info.minRowBytes());

    SkiaPaintCanvas canvas(surface->getCanvas());
    DrawHudContents(&canvas);
  }

  // Exports the backing to the ResourceProvider, giving it a ResourceId that
  // can be used in a DrawQuad.
  bool exported = pool_->PrepareForExport(pool_resource);
  DCHECK(exported);
  viz::ResourceId resource_id = pool_resource.resource_id_for_export();

  // Save the resource to prevent reuse until it is exported to the display
  // compositor. Next time we come here, we can release it back to the pool as
  // it will be exported by then.
  in_flight_resource_ = std::move(pool_resource);

  // This iterates over the RenderPass list of quads to find the HUD quad, which
  // will always be in the root RenderPass.
  auto& render_pass = list.back();
  for (auto it = render_pass->quad_list.begin();
       it != render_pass->quad_list.end(); ++it) {
    if (*it == current_quad_) {
      const viz::SharedQuadState* sqs = current_quad_->shared_quad_state;
      gfx::Rect quad_rect = current_quad_->rect;
      gfx::Rect visible_rect = current_quad_->visible_rect;
      current_quad_ = nullptr;

      auto* quad =
          render_pass->quad_list.ReplaceExistingElement<viz::TextureDrawQuad>(
              it);

      const float vertex_opacity[] = {1.f, 1.f, 1.f, 1.f};
      quad->SetNew(sqs, quad_rect, visible_rect, /*needs_blending=*/true,
                   resource_id, /*premultiplied_alpha=*/true,
                   /*uv_top_left=*/gfx::PointF(),
                   /*uv_bottom_right=*/gfx::PointF(1.f, 1.f),
                   /*background_color=*/SK_ColorTRANSPARENT, vertex_opacity,
                   /*flipped=*/false,
                   /*nearest_neighbor=*/false, /*secure_output_only=*/false,
                   gfx::ProtectedVideoType::kClear);
      ValidateQuadResources(quad);
      break;
    }
  }
  // If this fails, we didn't find |current_quad_| in the root RenderPass, so we
  // didn't append it for the frame (why are we here then?), or it landed in
  // some other RenderPass, both of which are unexpected.
  DCHECK(!current_quad_);
}

void HeadsUpDisplayLayerImpl::ReleaseResources() {
  if (in_flight_resource_)
    pool_->ReleaseResource(std::move(in_flight_resource_));
  pool_.reset();
}

gfx::Rect HeadsUpDisplayLayerImpl::GetEnclosingRectInTargetSpace() const {
  DCHECK_GT(internal_contents_scale_, 0.f);
  return GetScaledEnclosingRectInTargetSpace(internal_contents_scale_);
}

void HeadsUpDisplayLayerImpl::SetHUDTypeface(sk_sp<SkTypeface> typeface) {
  if (typeface_ == typeface)
    return;

  DCHECK(typeface_.get() == nullptr);
  typeface_ = std::move(typeface);
  NoteLayerPropertyChanged();
}

const std::vector<gfx::Rect>& HeadsUpDisplayLayerImpl::LayoutShiftRects()
    const {
  return layout_shift_rects_;
}

void HeadsUpDisplayLayerImpl::SetLayoutShiftRects(
    const std::vector<gfx::Rect>& rects) {
  layout_shift_rects_ = rects;
}

void HeadsUpDisplayLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_);
  layer_impl->SetLayoutShiftRects(layout_shift_rects_);
  layout_shift_rects_.clear();
}

void HeadsUpDisplayLayerImpl::UpdateHudContents() {
  const LayerTreeDebugState& debug_state = layer_tree_impl()->debug_state();

  // Don't update numbers every frame so text is readable.
  base::TimeTicks now = layer_tree_impl()->CurrentBeginFrameArgs().frame_time;
  if (base::TimeDelta(now - time_of_last_graph_update_).InSecondsF() > 0.25f) {
    time_of_last_graph_update_ = now;

    if (debug_state.show_fps_counter) {
      throughput_value_ =
          layer_tree_impl()->dropped_frame_counter()->GetAverageThroughput();
    }

    if (debug_state.ShowMemoryStats()) {
      MemoryHistory* memory_history = layer_tree_impl()->memory_history();
      if (memory_history->End())
        memory_entry_ = **memory_history->End();
      else
        memory_entry_ = MemoryHistory::Entry();
    }
  }
}

void HeadsUpDisplayLayerImpl::DrawHudContents(PaintCanvas* canvas) {
  const LayerTreeDebugState& debug_state = layer_tree_impl()->debug_state();

  TRACE_EVENT0("cc", "DrawHudContents");
  canvas->clear(SkColorSetARGB(0, 0, 0, 0));
  canvas->save();
  canvas->scale(internal_contents_scale_, internal_contents_scale_);

  if (debug_state.ShowHudRects()) {
    DrawDebugRects(canvas, layer_tree_impl()->debug_rect_history());
    if (IsAnimatingHUDContents()) {
      layer_tree_impl()->SetNeedsRedraw();
    }
  }

  if (!debug_state.show_fps_counter) {
    canvas->restore();
    return;
  }

  SkRect area = DrawFrameThroughputDisplay(
      canvas, layer_tree_impl()->dropped_frame_counter(), 0, 0);
  area = DrawGpuRasterizationStatus(canvas, 0, area.bottom(),
                                    std::max<SkScalar>(area.width(), 150));

  if (debug_state.ShowMemoryStats() && memory_entry_.total_bytes_used)
    DrawMemoryDisplay(canvas, 0, area.bottom(),
                      std::max<SkScalar>(area.width(), 150));

  canvas->restore();
}

void HeadsUpDisplayLayerImpl::DrawText(PaintCanvas* canvas,
                                       const PaintFlags& flags,
                                       const std::string& text,
                                       TextAlign align,
                                       int size,
                                       int x,
                                       int y) const {
  DCHECK(typeface_.get());
  SkFont font(typeface_, size);
  font.setEdging(SkFont::Edging::kAntiAlias);

  if (align == TextAlign::kCenter) {
    auto width =
        font.measureText(text.c_str(), text.length(), SkTextEncoding::kUTF8);
    x -= width * 0.5f;
  } else if (align == TextAlign::kRight) {
    auto width =
        font.measureText(text.c_str(), text.length(), SkTextEncoding::kUTF8);
    x -= width;
  }

  canvas->drawTextBlob(
      SkTextBlob::MakeFromText(text.c_str(), text.length(), font), x, y, flags);
}

void HeadsUpDisplayLayerImpl::DrawText(PaintCanvas* canvas,
                                       const PaintFlags& flags,
                                       const std::string& text,
                                       TextAlign align,
                                       int size,
                                       const SkPoint& pos) const {
  DrawText(canvas, flags, text, align, size, pos.x(), pos.y());
}

void HeadsUpDisplayLayerImpl::DrawGraphBackground(PaintCanvas* canvas,
                                                  PaintFlags* flags,
                                                  const SkRect& bounds) const {
  flags->setColor(DebugColors::HUDBackgroundColor());
  canvas->drawRect(bounds, *flags);
}

void HeadsUpDisplayLayerImpl::DrawGraphLines(PaintCanvas* canvas,
                                             PaintFlags* flags,
                                             const SkRect& bounds) const {
  // Draw top and bottom line.
  flags->setColor(DebugColors::HUDSeparatorLineColor());
  canvas->drawLine(bounds.left(), bounds.top() - 1, bounds.right(),
                   bounds.top() - 1, *flags);
  canvas->drawLine(bounds.left(), bounds.bottom(), bounds.right(),
                   bounds.bottom(), *flags);
}

SkRect HeadsUpDisplayLayerImpl::DrawFrameThroughputDisplay(
    PaintCanvas* canvas,
    const DroppedFrameCounter* dropped_frame_counter,
    int right,
    int top) const {
  const int kPadding = 4;
  const int kGap = 6;

  const int kTitleFontHeight = 13;
  const int kFontHeight = 12;

  const int kGraphWidth =
      base::saturated_cast<int>(dropped_frame_counter->frame_history_size());
  const int kGraphHeight = 40;

  int width = kGraphWidth + 4 * kPadding;
  int height = kTitleFontHeight + kFontHeight + kGraphHeight + 6 * kPadding + 2;
  int left = 0;
  SkRect area = SkRect::MakeXYWH(left, top, width, height);

  PaintFlags flags;
  DrawGraphBackground(canvas, &flags, area);

  SkRect title_bounds =
      SkRect::MakeXYWH(left + kPadding, top + kPadding, kGraphWidth + kGap + 2,
                       kTitleFontHeight);
  SkRect text_bounds =
      SkRect::MakeXYWH(left + kPadding, title_bounds.bottom() + 2 * kPadding,
                       kGraphWidth + kGap + 2, kFontHeight);
  SkRect graph_bounds =
      SkRect::MakeXYWH(left + kPadding, text_bounds.bottom() + 2 * kPadding,
                       kGraphWidth, kGraphHeight);

  // Draw the frame rendering stats.
  const std::string title("Frames");
  const std::string value_text = base::StringPrintf("%d%%", throughput_value_);
  const std::string dropped_frames_text =
      base::StringPrintf("%zu (%zu m) dropped of %zu",
                         dropped_frame_counter->total_compositor_dropped(),
                         dropped_frame_counter->total_main_dropped(),
                         dropped_frame_counter->total_frames());

  VLOG(1) << value_text;

  flags.setColor(DebugColors::HUDTitleColor());
  DrawText(canvas, flags, title, TextAlign::kLeft, kTitleFontHeight,
           title_bounds.left(), title_bounds.bottom());

  flags.setColor(DebugColors::FPSDisplayTextAndGraphColor());
  DrawText(canvas, flags, value_text, TextAlign::kLeft, kFontHeight,
           text_bounds.left(), text_bounds.bottom());
  DrawText(canvas, flags, dropped_frames_text, TextAlign::kRight, kFontHeight,
           text_bounds.right(), text_bounds.bottom());

  DrawGraphLines(canvas, &flags, graph_bounds);

  // Collect the frames graph data.
  SkPath good_path;
  SkPath dropped_path;
  SkPath partial_path;
  for (auto it = --dropped_frame_counter->end(); it; --it) {
    const auto state = **it;
    int x = graph_bounds.left() + it.index();
    SkPath& path = state == DroppedFrameCounter::kFrameStateDropped
                       ? dropped_path
                       : state == DroppedFrameCounter::kFrameStateComplete
                             ? good_path
                             : partial_path;
    path.moveTo(x, graph_bounds.top());
    path.lineTo(x, graph_bounds.bottom());
  }

  // Draw FPS graph.
  flags.setAntiAlias(true);
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);

  flags.setColor(DebugColors::FPSDisplaySuccessfulFrame());
  canvas->drawPath(good_path, flags);

  flags.setColor(DebugColors::FPSDisplayDroppedFrame());
  canvas->drawPath(dropped_path, flags);

  flags.setColor(DebugColors::FPSDisplayMissedFrame());
  canvas->drawPath(partial_path, flags);

  return area;
}

SkRect HeadsUpDisplayLayerImpl::DrawMemoryDisplay(PaintCanvas* canvas,
                                                  int right,
                                                  int top,
                                                  int width) const {
  const int kPadding = 4;
  const int kTitleFontHeight = 13;
  const int kFontHeight = 12;

  const int height = kTitleFontHeight + 2 * kFontHeight + 5 * kPadding;
  const int left = 0;
  const SkRect area = SkRect::MakeXYWH(left, top, width, height);

  const double kMegabyte = 1000.0 * 1000.0;

  PaintFlags flags;
  DrawGraphBackground(canvas, &flags, area);

  SkPoint title_pos =
      SkPoint::Make(left + kPadding, top + kFontHeight + kPadding);
  SkPoint stat1_pos = SkPoint::Make(left + width - kPadding - 1,
                                    top + kPadding + 2 * kFontHeight);
  SkPoint stat2_pos = SkPoint::Make(left + width - kPadding - 1,
                                    top + 2 * kPadding + 3 * kFontHeight);

  flags.setColor(DebugColors::HUDTitleColor());
  DrawText(canvas, flags, "GPU memory", TextAlign::kLeft, kTitleFontHeight,
           title_pos);

  flags.setColor(DebugColors::MemoryDisplayTextColor());
  std::string text = base::StringPrintf(
      "%6.1f MB used", memory_entry_.total_bytes_used / kMegabyte);
  DrawText(canvas, flags, text, TextAlign::kRight, kFontHeight, stat1_pos);

  if (!memory_entry_.had_enough_memory)
    flags.setColor(SK_ColorRED);
  text = base::StringPrintf("%6.1f MB max ",
                            memory_entry_.total_budget_in_bytes / kMegabyte);

  DrawText(canvas, flags, text, TextAlign::kRight, kFontHeight, stat2_pos);

  // Draw memory graph.
  int length = 2 * kFontHeight + kPadding + 12;
  SkRect oval =
      SkRect::MakeXYWH(left + kPadding * 6,
                       top + kTitleFontHeight + kPadding * 3, length, length);
  flags.setAntiAlias(true);
  flags.setStyle(PaintFlags::kFill_Style);

  flags.setColor(SkColorSetARGB(64, 255, 255, 0));
  DrawArc(canvas, oval, 180, 180, flags);

  int radius = length / 2;
  int cx = oval.left() + radius;
  int cy = oval.top() + radius;
  double angle = (static_cast<double>(memory_entry_.total_bytes_used) /
                  memory_entry_.total_budget_in_bytes) *
                 180;

  SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorGREEN,
                      SkColorSetARGB(255, 255, 140, 0), SK_ColorRED};
  const SkScalar pos[] = {SkFloatToScalar(0.2f), SkFloatToScalar(0.4f),
                          SkFloatToScalar(0.6f), SkFloatToScalar(0.8f),
                          SkFloatToScalar(1.0f)};
  flags.setShader(PaintShader::MakeSweepGradient(cx, cy, colors, pos, 5,
                                                 SkTileMode::kClamp, 0, 360));
  flags.setAntiAlias(true);

  // Draw current status.
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setAlpha(32);
  flags.setStrokeWidth(4);
  DrawArc(canvas, oval, 180, angle, flags);

  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColorSetARGB(255, 0, 255, 0));
  DrawArc(canvas, oval, 180, angle, flags);
  flags.setShader(nullptr);

  return area;
}

SkRect HeadsUpDisplayLayerImpl::DrawGpuRasterizationStatus(PaintCanvas* canvas,
                                                           int right,
                                                           int top,
                                                           int width) const {
  std::string status;
  SkColor color = SK_ColorRED;
  switch (layer_tree_impl()->GetGpuRasterizationStatus()) {
    case GpuRasterizationStatus::ON:
      status = "on";
      color = SK_ColorGREEN;
      break;
    case GpuRasterizationStatus::OFF_FORCED:
      status = "off (forced)";
      color = SK_ColorRED;
      break;
    case GpuRasterizationStatus::OFF_DEVICE:
      status = "off (device)";
      color = SK_ColorRED;
      break;
  }

  if (status.empty())
    return SkRect::MakeEmpty();

  const int kPadding = 4;
  const int kTitleFontHeight = 13;
  const int kFontHeight = 12;

  const int height = kTitleFontHeight + kFontHeight + 3 * kPadding;
  const int left = 0;
  const SkRect area = SkRect::MakeXYWH(left, top, width, height);

  PaintFlags flags;
  DrawGraphBackground(canvas, &flags, area);

  SkPoint gpu_status_pos = SkPoint::Make(left + width - kPadding,
                                         top + 2 * kFontHeight + 2 * kPadding);
  flags.setColor(DebugColors::HUDTitleColor());
  DrawText(canvas, flags, "GPU raster", TextAlign::kLeft, kTitleFontHeight,
           left + kPadding, top + kFontHeight + kPadding);
  flags.setColor(color);
  DrawText(canvas, flags, status, TextAlign::kRight, kFontHeight,
           gpu_status_pos);

  return area;
}

void HeadsUpDisplayLayerImpl::DrawDebugRect(
    PaintCanvas* canvas,
    PaintFlags* flags,
    const DebugRect& rect,
    SkColor stroke_color,
    SkColor fill_color,
    float stroke_width,
    const std::string& label_text) const {
  DCHECK(typeface_.get());
  gfx::Rect debug_layer_rect =
      gfx::ScaleToEnclosingRect(rect.rect, 1.0 / internal_contents_scale_,
                                1.0 / internal_contents_scale_);
  SkIRect sk_rect = RectToSkIRect(debug_layer_rect);
  flags->setColor(fill_color);
  flags->setStyle(PaintFlags::kFill_Style);
  canvas->drawIRect(sk_rect, *flags);

  flags->setColor(stroke_color);
  flags->setStyle(PaintFlags::kStroke_Style);
  flags->setStrokeWidth(SkFloatToScalar(stroke_width));
  canvas->drawIRect(sk_rect, *flags);

  if (label_text.length()) {
    const int kFontHeight = 12;
    const int kPadding = 3;

    // The debug_layer_rect may be huge, and converting to a floating point may
    // be lossy, so intersect with the HUD layer bounds first to prevent that.
    gfx::Rect clip_rect = debug_layer_rect;
    clip_rect.Intersect(gfx::Rect(internal_content_bounds_));
    SkRect sk_clip_rect = RectToSkRect(clip_rect);

    canvas->save();
    canvas->clipRect(sk_clip_rect);
    canvas->translate(sk_clip_rect.x(), sk_clip_rect.y());

    PaintFlags label_flags;
    label_flags.setColor(stroke_color);
    SkFont label_font(typeface_, kFontHeight);

    const SkScalar label_text_width = label_font.measureText(
        label_text.c_str(), label_text.length(), SkTextEncoding::kUTF8);
    canvas->drawRect(SkRect::MakeWH(label_text_width + 2 * kPadding,
                                    kFontHeight + 2 * kPadding),
                     label_flags);

    label_flags.setColor(SkColorSetARGB(255, 50, 50, 50));
    DrawText(canvas, label_flags, label_text, TextAlign::kLeft, kFontHeight,
             kPadding, kFontHeight * 0.8f + kPadding);
    canvas->restore();
  }
}

void HeadsUpDisplayLayerImpl::DrawDebugRects(
    PaintCanvas* canvas,
    DebugRectHistory* debug_rect_history) {
  PaintFlags flags;

  const std::vector<DebugRect>& debug_rects = debug_rect_history->debug_rects();
  std::vector<DebugRect> new_paint_rects;
  std::vector<DebugRect> new_layout_shift_rects;

  for (size_t i = 0; i < debug_rects.size(); ++i) {
    SkColor stroke_color = 0;
    SkColor fill_color = 0;
    float stroke_width = 0.f;
    std::string label_text;

    switch (debug_rects[i].type) {
      case LAYOUT_SHIFT_RECT_TYPE:
        new_layout_shift_rects.push_back(debug_rects[i]);
        continue;
      case PAINT_RECT_TYPE:
        new_paint_rects.push_back(debug_rects[i]);
        continue;
      case PROPERTY_CHANGED_RECT_TYPE:
        stroke_color = DebugColors::PropertyChangedRectBorderColor();
        fill_color = DebugColors::PropertyChangedRectFillColor();
        stroke_width = DebugColors::PropertyChangedRectBorderWidth();
        break;
      case SURFACE_DAMAGE_RECT_TYPE:
        stroke_color = DebugColors::SurfaceDamageRectBorderColor();
        fill_color = DebugColors::SurfaceDamageRectFillColor();
        stroke_width = DebugColors::SurfaceDamageRectBorderWidth();
        break;
      case SCREEN_SPACE_RECT_TYPE:
        stroke_color = DebugColors::ScreenSpaceLayerRectBorderColor();
        fill_color = DebugColors::ScreenSpaceLayerRectFillColor();
        stroke_width = DebugColors::ScreenSpaceLayerRectBorderWidth();
        break;
      case TOUCH_EVENT_HANDLER_RECT_TYPE:
        stroke_color = DebugColors::TouchEventHandlerRectBorderColor();
        fill_color = DebugColors::TouchEventHandlerRectFillColor();
        stroke_width = DebugColors::TouchEventHandlerRectBorderWidth();
        label_text = "touch event listener: ";
        label_text.append(TouchActionToString(debug_rects[i].touch_action));
        break;
      case WHEEL_EVENT_HANDLER_RECT_TYPE:
        stroke_color = DebugColors::WheelEventHandlerRectBorderColor();
        fill_color = DebugColors::WheelEventHandlerRectFillColor();
        stroke_width = DebugColors::WheelEventHandlerRectBorderWidth();
        label_text = "mousewheel event listener";
        break;
      case SCROLL_EVENT_HANDLER_RECT_TYPE:
        stroke_color = DebugColors::ScrollEventHandlerRectBorderColor();
        fill_color = DebugColors::ScrollEventHandlerRectFillColor();
        stroke_width = DebugColors::ScrollEventHandlerRectBorderWidth();
        label_text = "scroll event listener";
        break;
      case NON_FAST_SCROLLABLE_RECT_TYPE:
        stroke_color = DebugColors::NonFastScrollableRectBorderColor();
        fill_color = DebugColors::NonFastScrollableRectFillColor();
        stroke_width = DebugColors::NonFastScrollableRectBorderWidth();
        label_text = "repaints on scroll";
        break;
      case MAIN_THREAD_SCROLLING_REASON_RECT_TYPE:
        stroke_color = DebugColors::MainThreadScrollingReasonRectBorderColor();
        fill_color = DebugColors::MainThreadScrollingReasonRectFillColor();
        stroke_width = DebugColors::MainThreadScrollingReasonRectBorderWidth();
        label_text = "main thread scrolling: ";
        label_text.append(base::ToLowerASCII(MainThreadScrollingReason::AsText(
            debug_rects[i].main_thread_scrolling_reasons)));
        break;
      case ANIMATION_BOUNDS_RECT_TYPE:
        stroke_color = DebugColors::LayerAnimationBoundsBorderColor();
        fill_color = DebugColors::LayerAnimationBoundsFillColor();
        stroke_width = DebugColors::LayerAnimationBoundsBorderWidth();
        label_text = "animation bounds";
        break;
    }

    DrawDebugRect(canvas, &flags, debug_rects[i], stroke_color, fill_color,
                  stroke_width, label_text);
  }

  if (new_paint_rects.size()) {
    paint_rects_.swap(new_paint_rects);
    paint_rects_fade_step_ = DebugColors::kFadeSteps;
  }
  if (paint_rects_fade_step_ > 0) {
    paint_rects_fade_step_--;
    for (size_t i = 0; i < paint_rects_.size(); ++i) {
      DrawDebugRect(canvas, &flags, paint_rects_[i],
                    DebugColors::PaintRectBorderColor(paint_rects_fade_step_),
                    DebugColors::PaintRectFillColor(paint_rects_fade_step_),
                    DebugColors::PaintRectBorderWidth(), "");
    }
  }

  if (new_layout_shift_rects.size()) {
    layout_shift_debug_rects_.swap(new_layout_shift_rects);
    layout_shift_rects_fade_step_ = DebugColors::kFadeSteps;
  }
  if (layout_shift_rects_fade_step_ > 0) {
    layout_shift_rects_fade_step_--;
    for (size_t i = 0; i < layout_shift_debug_rects_.size(); ++i) {
      DrawDebugRect(
          canvas, &flags, layout_shift_debug_rects_[i],
          DebugColors::LayoutShiftRectBorderColor(),
          DebugColors::LayoutShiftRectFillColor(layout_shift_rects_fade_step_),
          DebugColors::LayoutShiftRectBorderWidth(), "");
    }
  }
}

const char* HeadsUpDisplayLayerImpl::LayerTypeAsString() const {
  return "cc::HeadsUpDisplayLayerImpl";
}

void HeadsUpDisplayLayerImpl::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetString("layer_name", "Heads Up Display Layer");
}

}  // namespace cc
