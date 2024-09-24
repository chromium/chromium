// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iomanip>
#include <optional>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/debug/debug_colors.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/resources/memory_history.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
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
  }
};

#if BUILDFLAG(IS_ANDROID)
struct MetricsDrawSizes {
  const int kTopPadding = 35;
  const int kPadding = 15;
  const int kFontHeight = 32;
  const int kWidth = 525;
  const int kSidePadding = 20;
  const int kBadgeWidth = 25;
} constexpr metrics_sizes;
#else
struct MetricsDrawSizes {
  const int kTopPadding = 35;
  const int kPadding = 15;
  const int kFontHeight = 22;
  const int kWidth = 425;
  const int kSidePadding = 20;
  const int kBadgeWidth = 25;
} constexpr metrics_sizes;
#endif

}  // namespace

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    const std::string& paused_localized_message)
    : LayerImpl(tree_impl, id),
      paused_localized_message_(paused_localized_message) {}

HeadsUpDisplayLayerImpl::~HeadsUpDisplayLayerImpl() {
  ReleaseResources();
}

mojom::LayerType HeadsUpDisplayLayerImpl::GetLayerType() const {
  return mojom::LayerType::kHeadsUpDisplay;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id(),
                                         paused_localized_message_);
}

class HudGpuBacking : public ResourcePool::GpuBacking {
 public:
  ~HudGpuBacking() override {
    if (!shared_image) {
      return;
    }
    if (returned_sync_token.HasData())
      shared_image_interface->DestroySharedImage(returned_sync_token,
                                                 std::move(shared_image));
    else if (mailbox_sync_token.HasData())
      shared_image_interface->DestroySharedImage(mailbox_sync_token,
                                                 std::move(shared_image));
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
    if (!shared_image) {
      return;
    }

    auto tracing_guid = shared_image->GetGUIDForTracing();
    pmd->CreateSharedGlobalAllocatorDump(tracing_guid);
    pmd->AddOwnershipEdge(buffer_dump_guid, tracing_guid, importance);
  }

  raw_ptr<gpu::SharedImageInterface> shared_image_interface = nullptr;
};

class HudSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~HudSoftwareBacking() override {
    if (shared_image) {
      auto sii = layer_tree_frame_sink->shared_image_interface();
      if (sii) {
        sii->DestroySharedImage(mailbox_sync_token, std::move(shared_image));
      }
    } else {
      layer_tree_frame_sink->DidDeleteSharedBitmap(shared_bitmap_id);
    }
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
      pmd->CreateSharedMemoryOwnershipEdge(buffer_dump_guid,
                                           shared_mapping.guid(), importance);
  }

  raw_ptr<LayerTreeFrameSink> layer_tree_frame_sink;
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
  // TODO(crbug.com/40176440): Support 2D scales in heads up layers.
  internal_contents_scale_ = GetIdealContentsScaleKey();
  internal_content_bounds_ =
      gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_);
  internal_content_bounds_.SetToMin(
      gfx::Size(max_texture_size, max_texture_size));

  return true;
}

void HeadsUpDisplayLayerImpl::DidDraw(
    viz::ClientResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);
  // We always clear `placeholder_quad_` as drawing may get skipped and
  // `UpdateHudTexture` might not get called.
  placeholder_quad_ = nullptr;
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
  quad->SetNew(shared_quad_state, quad_rect, quad_rect, SkColors::kTransparent,
               false);
  ValidateQuadResources(quad);
  placeholder_quad_ = quad;
}

void HeadsUpDisplayLayerImpl::UpdateHudTexture(
    DrawMode draw_mode,
    LayerTreeFrameSink* layer_tree_frame_sink,
    viz::ClientResourceProvider* resource_provider,
    const RasterCapabilities& raster_caps,
    const viz::CompositorRenderPassList& list) {
  viz::DrawQuad* hud_quad = placeholder_quad_;
  // The `placeholder_quad_` is only valid for the currently drawing RenderPass,
  // and we need to get a new pointer for the next frame. It would become
  // dangling after drawing completes.
  placeholder_quad_ = nullptr;

  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    return;
  }

  // Update state that will be drawn.
  UpdateHudContents();

  viz::RasterContextProvider* raster_context_provider = nullptr;
  std::optional<viz::RasterContextProvider::ScopedRasterContextLock> lock;
  if (draw_mode == DRAW_MODE_HARDWARE) {
    // TODO(penghuang): It would be better to use context_provider() instead of
    // worker_context_provider() if/when it's switched to RasterContextProvider.
    raster_context_provider = layer_tree_frame_sink->worker_context_provider();
    CHECK(raster_context_provider);
    lock.emplace(raster_context_provider);
  }

  if (!pool_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        layer_tree_impl()->task_runner_provider()->HasImplThread()
            ? layer_tree_impl()->task_runner_provider()->ImplThreadTaskRunner()
            : layer_tree_impl()->task_runner_provider()->MainThreadTaskRunner();
    pool_ = std::make_unique<ResourcePool>(
        resource_provider, layer_tree_frame_sink->context_provider(),
        std::move(task_runner), ResourcePool::kDefaultExpirationDelay,
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
  bool needs_clear = false;
  if (draw_mode == DRAW_MODE_HARDWARE) {
    pool_resource = pool_->AcquireResource(
        internal_content_bounds_, raster_caps.tile_format, gfx::ColorSpace());

    if (!pool_resource.gpu_backing()) {
      auto backing = std::make_unique<HudGpuBacking>();
      auto* sii = raster_context_provider->SharedImageInterface();
      backing->shared_image_interface = sii;
      backing->overlay_candidate = raster_caps.tile_overlay_candidate;

      gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                       gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
      if (raster_caps.use_gpu_rasterization) {
        flags |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      }
      if (backing->overlay_candidate) {
        flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      }
      backing->shared_image = sii->CreateSharedImage(
          {pool_resource.format(), pool_resource.size(),
           pool_resource.color_space(), flags, "HeadsUpDisplayLayer"},
          gpu::kNullSurfaceHandle);
      CHECK(backing->shared_image);
      auto* ri = raster_context_provider->RasterInterface();
      ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
      pool_resource.set_gpu_backing(std::move(backing));
      needs_clear = true;
    } else if (pool_resource.gpu_backing()->returned_sync_token.HasData()) {
      auto* ri = raster_context_provider->RasterInterface();
      ri->WaitSyncTokenCHROMIUM(
          pool_resource.gpu_backing()->returned_sync_token.GetConstData());
      pool_resource.gpu_backing()->returned_sync_token = gpu::SyncToken();
    }
  } else {
    DCHECK_EQ(draw_mode, DRAW_MODE_SOFTWARE);

    auto sii = layer_tree_frame_sink->shared_image_interface();
    if (sii) {
      pool_resource = pool_->AcquireResource(internal_content_bounds_,
                                             viz::SinglePlaneFormat::kBGRA_8888,
                                             gfx::ColorSpace());

      if (!pool_resource.software_backing()) {
        auto backing = std::make_unique<HudSoftwareBacking>();
        backing->layer_tree_frame_sink = layer_tree_frame_sink;
        auto shared_image_mapping = sii->CreateSharedImage(
            {pool_resource.format(), pool_resource.size(),
             pool_resource.color_space(), gpu::SHARED_IMAGE_USAGE_CPU_WRITE,
             "HeadsUpDisplayLayer"});

        backing->shared_image = std::move(shared_image_mapping.shared_image);
        backing->shared_mapping = std::move(shared_image_mapping.mapping);
        CHECK(backing->shared_image);
        pool_resource.set_software_backing(std::move(backing));
      }

    } else {
      pool_resource = pool_->AcquireResource(internal_content_bounds_,
                                             viz::SinglePlaneFormat::kRGBA_8888,
                                             gfx::ColorSpace());
      if (!pool_resource.software_backing()) {
        auto backing = std::make_unique<HudSoftwareBacking>();
        backing->layer_tree_frame_sink = layer_tree_frame_sink;
        backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
        base::MappedReadOnlyRegion shm =
            viz::bitmap_allocation::AllocateSharedBitmap(
                pool_resource.size(), pool_resource.format());
        backing->shared_mapping = std::move(shm.mapping);

        layer_tree_frame_sink->DidAllocateSharedBitmap(
            std::move(shm.region), backing->shared_bitmap_id);
        pool_resource.set_software_backing(std::move(backing));
      }
    }
  }

  if (draw_mode == DRAW_MODE_HARDWARE) {
    DCHECK(pool_resource.gpu_backing());
    auto* backing = static_cast<HudGpuBacking*>(pool_resource.gpu_backing());
    auto* ri = raster_context_provider->RasterInterface();

    if (raster_caps.use_gpu_rasterization) {
      // If using |gpu_raster|, DrawHudContents() directly to a gpu texture
      // which is wrapped in an SkSurface.
      const auto& size = pool_resource.size();
      RecordPaintCanvas canvas;
      DrawHudContents(&canvas);
      auto display_item_list = base::MakeRefCounted<DisplayItemList>();
      display_item_list->StartPaint();
      display_item_list->push<DrawRecordOp>(canvas.ReleaseAsRecord());
      display_item_list->EndPaintOfUnpaired(gfx::Rect(size));
      display_item_list->Finalize();

      constexpr SkColor4f background_color = SkColors::kTransparent;
      constexpr GLuint msaa_sample_count = -1;
      constexpr bool can_use_lcd_text = true;
      ri->BeginRasterCHROMIUM(background_color, needs_clear, msaa_sample_count,
                              gpu::raster::kNoMSAA, can_use_lcd_text,
                              /*visible=*/true, gfx::ColorSpace::CreateSRGB(),
                              /*hdr_headroom=*/1.f,
                              backing->shared_image->mailbox().name);
      constexpr gfx::Vector2dF post_translate(0.f, 0.f);
      constexpr gfx::Vector2dF post_scale(1.f, 1.f);
      DummyImageProvider image_provider;
      size_t max_op_size_limit =
          gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
      ri->RasterCHROMIUM(
          display_item_list.get(), &image_provider, size, gfx::Rect(size),
          gfx::Rect(size), post_translate, post_scale, /*requires_clear=*/false,
          /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_limit);
      ri->EndRasterCHROMIUM();
    } else {
      // If not using |gpu_raster| but using gpu compositing, DrawHudContents()
      // into a software bitmap and upload it to a texture for compositing.
      if (!staging_surface_ ||
          gfx::SkISizeToSize(
              staging_surface_->getCanvas()->getBaseLayerSize()) !=
              pool_resource.size()) {
        SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
        staging_surface_ = SkSurfaces::Raster(
            SkImageInfo::MakeN32Premul(pool_resource.size().width(),
                                       pool_resource.size().height()),
            &props);
      }

      SkiaPaintCanvas canvas(staging_surface_->getCanvas());
      DrawHudContents(&canvas);

      TRACE_EVENT0("cc", "UploadHudTexture");
      SkPixmap pixmap;
      staging_surface_->peekPixels(&pixmap);

      uint32_t texture_target = backing->shared_image->GetTextureTarget();
      ri->WritePixels(backing->shared_image->mailbox(), /*dst_x_offset=*/0,
                      /*dst_y_offset=*/0, texture_target, pixmap);
    }

    backing->mailbox_sync_token =
        viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
  } else {
    // If not using gpu compositing, we DrawHudContents() directly into a shared
    // memory bitmap, wrapped in an SkSurface, that can be shared to the display
    // compositor.
    DCHECK_EQ(draw_mode, DRAW_MODE_SOFTWARE);
    DCHECK(pool_resource.software_backing());

    SkImageInfo info = SkImageInfo::MakeN32Premul(
        pool_resource.size().width(), pool_resource.size().height());
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    const size_t row_bytes = info.minRowBytes();
    auto* backing =
        static_cast<HudSoftwareBacking*>(pool_resource.software_backing());
    base::span<uint8_t> mem(backing->shared_mapping);
    CHECK_GE(mem.size(), info.computeByteSize(row_bytes));
    sk_sp<SkSurface> surface =
        SkSurfaces::WrapPixels(info, mem.data(), row_bytes, &props);

    SkiaPaintCanvas canvas(surface->getCanvas());
    DrawHudContents(&canvas);

    auto sii = layer_tree_frame_sink->shared_image_interface();
    if (backing->shared_image && sii) {
      backing->mailbox_sync_token = sii->GenVerifiedSyncToken();
    }
  }

  // Exports the backing to the ResourceProvider, giving it a ResourceId that
  // can be used in a DrawQuad.
  bool exported = pool_->PrepareForExport(
      pool_resource,
      viz::TransferableResource::ResourceSource::kHeadsUpDisplay);
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
    if (*it == hud_quad) {
      const viz::SharedQuadState* sqs = hud_quad->shared_quad_state;
      gfx::Rect quad_rect = hud_quad->rect;
      gfx::Rect visible_rect = hud_quad->visible_rect;

      auto* quad =
          render_pass->quad_list.ReplaceExistingElement<viz::TextureDrawQuad>(
              it);

      // The acquired resource's size could be bigger than actually needed due
      // to reuse. In this case, only use the part of the texture that is within
      // the bounds.
      gfx::PointF uv_bottom_right(1.f, 1.f);
      if (in_flight_resource_.size() != internal_content_bounds_) {
        uv_bottom_right.set_x(
            static_cast<double>(internal_content_bounds_.width()) /
            static_cast<double>(in_flight_resource_.size().width()));
        uv_bottom_right.set_y(
            static_cast<double>(internal_content_bounds_.height()) /
            static_cast<double>(in_flight_resource_.size().height()));
      }
      quad->SetNew(sqs, quad_rect, visible_rect, /*needs_blending=*/true,
                   resource_id, /*premultiplied_alpha=*/true,
                   /*uv_top_left=*/gfx::PointF(),
                   /*uv_bottom_right=*/uv_bottom_right,
                   /*background_color=*/SkColors::kTransparent,
                   /*flipped=*/false,
                   /*nearest_neighbor=*/false, /*secure_output_only=*/false,
                   gfx::ProtectedVideoType::kClear);
      ValidateQuadResources(quad);
      break;
    }
  }
}

void HeadsUpDisplayLayerImpl::ReleaseResources() {
  if (in_flight_resource_)
    pool_->ReleaseResource(std::move(in_flight_resource_));
  pool_.reset();
}

gfx::Rect HeadsUpDisplayLayerImpl::GetEnclosingVisibleRectInTargetSpace()
    const {
  DCHECK_GT(internal_contents_scale_, 0.f);
  return GetScaledEnclosingVisibleRectInTargetSpace(internal_contents_scale_);
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

void HeadsUpDisplayLayerImpl::ClearLayoutShiftRects() {
  layout_shift_rects_.clear();
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
      const auto& args = layer_tree_impl()->CurrentBeginFrameArgs();
      if (args.IsValid())
        frame_interval_ = args.interval;
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
  canvas->clear(SkColors::kTransparent);
  canvas->save();
  canvas->scale(internal_contents_scale_);

  if (debug_state.debugger_paused) {
    DrawDebuggerPaused(canvas);
    canvas->restore();
    return;
  }

  if (debug_state.ShowDebugRects()) {
    DrawDebugRects(canvas, layer_tree_impl()->debug_rect_history());
    if (IsAnimatingHUDContents()) {
      layer_tree_impl()->SetNeedsRedraw();
    }
  }

  if (!debug_state.ShouldDrawHudInfo()) {
    canvas->restore();
    return;
  }

  // Our output should be in layout space, but all of the draw commands for the
  // HUD overlays here are in dips. Scale the canvas to account for this
  // difference.
  canvas->scale(layer_tree_impl()->painted_device_scale_factor());

  SkRect area = SkRect::MakeXYWH(0, 0, 0, 0);

  if (debug_state.show_fps_counter) {
    area = DrawFrameThroughputDisplay(
        canvas, layer_tree_impl()->dropped_frame_counter(), 0, 0);
    area = DrawGpuRasterizationStatus(canvas, 0, area.bottom(),
                                      std::max<SkScalar>(area.width(), 150));
  }

  if (debug_state.ShowMemoryStats() && memory_entry_.total_bytes_used) {
    area = DrawMemoryDisplay(canvas, 0, area.bottom(),
                             std::max<SkScalar>(area.width(), 150));
  }

  // For the web vital and smoothness HUD on the top right corner, if the width
  // of the screen is smaller than the default width of the HUD, scale it down.
  if (bounds_width_in_dips() < metrics_sizes.kWidth) {
    double scale_to_bounds = static_cast<double>(bounds_width_in_dips()) /
                             static_cast<double>(metrics_sizes.kWidth);
    canvas->scale(scale_to_bounds, scale_to_bounds);
  }

  canvas->restore();
}

void HeadsUpDisplayLayerImpl::DrawDebuggerPaused(PaintCanvas* canvas) {
  SkColor4f background{0.0f, 0.0f, 0.0f, 0.35f};
  canvas->clear(background);

  const int kPadding = 4;
  const int kFontHeight = 12;

  PaintFlags label_flags;
  label_flags.setColor(SkColorSetARGB(255, 255, 255, 194));
  SkFont label_font(typeface_, kFontHeight);

  const SkScalar label_text_width = label_font.measureText(
      paused_localized_message_.c_str(), paused_localized_message_.length(),
      SkTextEncoding::kUTF8);

  canvas->save();

  gfx::Size space = internal_content_bounds_;
  space.Enlarge(-(label_text_width + 2 * kPadding), 0);
  canvas->translate(space.width() / 2, kFontHeight * 2);
  canvas->drawRect(SkRect::MakeWH(label_text_width + 2 * kPadding,
                                  kFontHeight + 2 * kPadding),
                   label_flags);

  label_flags.setColor(SkColorSetARGB(255, 50, 50, 50));
  DrawText(canvas, label_flags, paused_localized_message_, TextAlign::kLeft,
           kFontHeight, kPadding, kFontHeight * 0.8f + kPadding);
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

void HeadsUpDisplayLayerImpl::DrawSeparatorLine(PaintCanvas* canvas,
                                                PaintFlags* flags,
                                                const SkRect& bounds) const {
  // Draw separator line as transparent white.
  flags->setColor({1.0f, 1.0f, 1.0f, 0.25f});
  canvas->drawLine(bounds.left(), bounds.top(), bounds.right(), bounds.top(),
                   *flags);
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
  const std::string title("Frame Rate");
  std::string value_text = "n/a";
  if (frame_interval_.has_value()) {
    // This assumes a constant frame rate. If the frame rate changed throughout
    // the sequence, then maybe we should average over the sequence.
    double frame_rate = static_cast<double>(throughput_value_) /
                        (100 * frame_interval_.value().InSecondsF());
    value_text = base::StringPrintf("%5.1f fps", frame_rate);
  }
  VLOG(1) << value_text;

  flags.setColor(DebugColors::HUDTitleColor());
  DrawText(canvas, flags, title, TextAlign::kLeft, kTitleFontHeight,
           title_bounds.left(), title_bounds.bottom());

  flags.setColor(DebugColors::FPSDisplayTextAndGraphColor());
  DrawText(canvas, flags, value_text, TextAlign::kRight, kFontHeight,
           text_bounds.right(), text_bounds.bottom());

  DrawGraphLines(canvas, &flags, graph_bounds);

  // Collect the frames graph data.
  SkPath good_path;
  SkPath dropped_path;
  SkPath partial_path;
  for (auto it = dropped_frame_counter->End(); it; --it) {
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

  SkColor4f colors[] = {SkColors::kRed,
                        SkColors::kGreen,
                        SkColors::kGreen,
                        {1.0f, 0.55f, 0.0f, 1.0f},
                        SkColors::kRed};
  const SkScalar pos[] = {SkFloatToScalar(0.2f), SkFloatToScalar(0.4f),
                          SkFloatToScalar(0.6f), SkFloatToScalar(0.8f),
                          SkFloatToScalar(1.0f)};
  flags.setShader(PaintShader::MakeSweepGradient(cx, cy, colors, pos, 5,
                                                 SkTileMode::kClamp, 0, 360));
  flags.setAntiAlias(true);

  // Draw current status.
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setAlphaf(32.0f / 255.0f);
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
  if (layer_tree_impl()->use_gpu_rasterization()) {
    status = "on";
    color = SK_ColorGREEN;
  } else {
    status = "off";
    color = SK_ColorRED;
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
  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  flags.setColor(DebugColors::HUDTitleColor().toSkColor());
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
    SkColor4f stroke_color,
    SkColor4f fill_color,
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
    SkColor4f stroke_color = SkColors::kTransparent;
    SkColor4f fill_color = SkColors::kTransparent;
    float stroke_width = 0.f;
    std::string label_text;

    switch (debug_rects[i].type) {
      case DebugRectType::kLayoutShift:
        new_layout_shift_rects.push_back(debug_rects[i]);
        continue;
      case DebugRectType::kPaint:
        new_paint_rects.push_back(debug_rects[i]);
        continue;
      case DebugRectType::kPropertyChanged:
        stroke_color = DebugColors::PropertyChangedRectBorderColor();
        fill_color = DebugColors::PropertyChangedRectFillColor();
        stroke_width = DebugColors::PropertyChangedRectBorderWidth();
        break;
      case DebugRectType::kSurfaceDamage:
        stroke_color = DebugColors::SurfaceDamageRectBorderColor();
        fill_color = DebugColors::SurfaceDamageRectFillColor();
        stroke_width = DebugColors::SurfaceDamageRectBorderWidth();
        break;
      case DebugRectType::kScreenSpace:
        stroke_color = DebugColors::ScreenSpaceLayerRectBorderColor();
        fill_color = DebugColors::ScreenSpaceLayerRectFillColor();
        stroke_width = DebugColors::ScreenSpaceLayerRectBorderWidth();
        break;
      case DebugRectType::kTouchEventHandler:
        stroke_color = DebugColors::TouchEventHandlerRectBorderColor();
        fill_color = DebugColors::TouchEventHandlerRectFillColor();
        stroke_width = DebugColors::TouchEventHandlerRectBorderWidth();
        label_text = "touch event listener: ";
        label_text.append(TouchActionToString(debug_rects[i].touch_action));
        break;
      case DebugRectType::kWheelEventHandler:
        stroke_color = DebugColors::WheelEventHandlerRectBorderColor();
        fill_color = DebugColors::WheelEventHandlerRectFillColor();
        stroke_width = DebugColors::WheelEventHandlerRectBorderWidth();
        label_text = "mousewheel event listener";
        break;
      case DebugRectType::kScrollEventHandler:
        stroke_color = DebugColors::ScrollEventHandlerRectBorderColor();
        fill_color = DebugColors::ScrollEventHandlerRectFillColor();
        stroke_width = DebugColors::ScrollEventHandlerRectBorderWidth();
        label_text = "scroll event listener";
        break;
      case DebugRectType::kMainThreadScrollHitTest:
        stroke_color = DebugColors::MainThreadScrollHitTestRectBorderColor();
        fill_color = DebugColors::MainThreadScrollHitTestRectFillColor();
        stroke_width = DebugColors::MainThreadScrollHitTestRectBorderWidth();
        label_text = "main thread scroll hit test";
        break;
      case DebugRectType::kMainThreadScrollRepaint:
        stroke_color = DebugColors::MainThreadScrollRepaintRectBorderColor();
        fill_color = DebugColors::MainThreadScrollRepaintRectFillColor();
        stroke_width = DebugColors::MainThreadScrollRepaintRectBorderWidth();
        label_text = "main thread scroll repaint: ";
        label_text.append(base::ToLowerASCII(MainThreadScrollingReason::AsText(
            debug_rects[i].main_thread_scroll_repaint_reasons)));
        break;
      case DebugRectType::kRasterInducingScroll:
        stroke_color = DebugColors::RasterInducingScrollRectBorderColor();
        fill_color = DebugColors::RasterInducingScrollRectFillColor();
        stroke_width = DebugColors::RasterInducingScrollRectBorderWidth();
        label_text = "raster-inducing scroll (not bad)";
        break;
      case DebugRectType::kAnimationBounds:
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
    for (auto& paint_rect : paint_rects_) {
      DrawDebugRect(canvas, &flags, paint_rect,
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
    for (auto& layout_shift_debug_rect : layout_shift_debug_rects_) {
      // TODO(crbug.com/40219248): Remove all instances of toSkColor below and
      // make all SkColor4f.
      DrawDebugRect(
          canvas, &flags, layout_shift_debug_rect,
          DebugColors::LayoutShiftRectBorderColor(),
          DebugColors::LayoutShiftRectFillColor(layout_shift_rects_fade_step_),
          DebugColors::LayoutShiftRectBorderWidth(), "");
    }
  }
}

void HeadsUpDisplayLayerImpl::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetString("layer_name", "Heads Up Display Layer");
}

}  // namespace cc
