// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <bit>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "cc/raster/raster_source.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "url/gurl.h"

namespace cc {

// Subclass for InUsePoolResource that holds ownership of a gpu-rastered backing
// and does cleanup of the backing when destroyed.
class GpuRasterBufferProvider::GpuRasterBacking
    : public ResourcePool::GpuBacking {
 public:
  ~GpuRasterBacking() override {
    if (!shared_image) {
      return;
    }
    auto* sii = worker_context_provider->SharedImageInterface();
    if (returned_sync_token.HasData())
      sii->DestroySharedImage(returned_sync_token, std::move(shared_image));
    else if (mailbox_sync_token.HasData())
      sii->DestroySharedImage(mailbox_sync_token, std::move(shared_image));
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

  // The context used to clean up the mailbox
  raw_ptr<viz::RasterContextProvider> worker_context_provider = nullptr;
};

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    const ResourcePool::InUsePoolResource& in_use_resource,
    GpuRasterBacking* backing,
    bool resource_has_previous_content,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates)
    : client_(client),
      backing_(backing),
      resource_size_(in_use_resource.size()),
      shared_image_format_(in_use_resource.format()),
      color_space_(in_use_resource.color_space()),
      resource_has_previous_content_(resource_has_previous_content),
      depends_on_at_raster_decodes_(depends_on_at_raster_decodes),
      depends_on_hardware_accelerated_jpeg_candidates_(
          depends_on_hardware_accelerated_jpeg_candidates),
      depends_on_hardware_accelerated_webp_candidates_(
          depends_on_hardware_accelerated_webp_candidates) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only do this in Chrome OS because:
  //   1) We will use this timestamp to measure raster scheduling delay and we
  //      only need to collect that data to assess the impact of hardware
  //      acceleration of image decodes which works only on Chrome OS.
  //   2) We use CLOCK_MONOTONIC in that OS to get timestamps, so we can assert
  //      certain assumptions.
  creation_time_ = base::TimeTicks::Now();
#endif
}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() = default;

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      client_->worker_context_provider_, url.possibly_invalid_spec().c_str());
  gpu::raster::RasterInterface* ri =
      client_->worker_context_provider_->RasterInterface();
  PlaybackOnWorkerThread(raster_source, raster_full_rect, raster_dirty_rect,
                         new_content_id, transform, playback_settings, url);

  backing_->mailbox_sync_token =
      viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
  backing_->returned_sync_token = gpu::SyncToken();
}

bool GpuRasterBufferProvider::RasterBufferImpl::
    SupportsBackgroundThreadPriority() const {
  return true;
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    viz::RasterContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    const RasterCapabilities& raster_caps,
    const gfx::Size& max_tile_size,
    bool unpremultiply_and_dither_low_bit_depth_tiles,
    RasterQueryQueue* const pending_raster_queries,
    float raster_metric_probability)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      tile_format_(raster_caps.tile_format),
      tile_overlay_candidate_(raster_caps.tile_overlay_candidate),
      max_tile_size_(max_tile_size),
      pending_raster_queries_(pending_raster_queries),
      raster_metric_probability_(raster_metric_probability),
      is_using_raw_draw_(features::IsUsingRawDraw()),
      is_using_dmsaa_(
          base::FeatureList::IsEnabled(features::kUseDMSAAForTiles)) {
  DCHECK(pending_raster_queries);
  DCHECK(compositor_context_provider);
  CHECK(worker_context_provider);

#if BUILDFLAG(IS_ANDROID)
  {
    std::optional<viz::RasterContextProvider::ScopedRasterContextLock> lock;
    lock.emplace(worker_context_provider);
    auto is_using_vulkan =
        worker_context_provider->ContextCapabilities().using_vulkan_context;

    // On Android, DMSAA on vulkan backend launch is controlled by
    // kUseDMSAAForTiles whereas GL backend launch is controlled by
    // kUseDMSAAForTilesAndroidGL.
    is_using_dmsaa_ =
        (base::FeatureList::IsEnabled(features::kUseDMSAAForTiles) &&
         is_using_vulkan) ||
        (base::FeatureList::IsEnabled(features::kUseDMSAAForTilesAndroidGL) &&
         !is_using_vulkan);
  }
#endif
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<GpuRasterBacking>();
    backing->worker_context_provider = worker_context_provider_;
    backing->overlay_candidate = tile_overlay_candidate_;
    backing->is_using_raw_draw =
        !backing->overlay_candidate && is_using_raw_draw_;
    resource.set_gpu_backing(std::move(backing));
  }
  GpuRasterBacking* backing =
      static_cast<GpuRasterBacking*>(resource.gpu_backing());
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<RasterBufferImpl>(
      this, resource, backing, resource_has_previous_content,
      depends_on_at_raster_decodes,
      depends_on_hardware_accelerated_jpeg_candidates,
      depends_on_hardware_accelerated_webp_candidates);
}

void GpuRasterBufferProvider::Flush() {
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

viz::SharedImageFormat GpuRasterBufferProvider::GetFormat() const {
  return tile_format_;
}

bool GpuRasterBufferProvider::IsResourcePremultiplied() const {
  return !ShouldUnpremultiplyAndDitherResource(GetFormat());
}

bool GpuRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) {
  FlushIfNeeded();
  const gpu::SyncToken& sync_token = resource.gpu_backing()->mailbox_sync_token;
  // This SyncToken() should have been set by calling OrderingBarrier() before
  // calling this.
  DCHECK(sync_token.HasData());

  // IsSyncTokenSignaled is thread-safe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignaled(
      sync_token);
}

bool GpuRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) {
  FlushIfNeeded();
  gpu::SyncToken latest_sync_token;
  for (const auto* in_use : resources) {
    const gpu::SyncToken& sync_token =
        in_use->gpu_backing()->mailbox_sync_token;
    if (sync_token.release_count() > latest_sync_token.release_count())
      latest_sync_token = sync_token;
  }
  uint64_t callback_id = latest_sync_token.release_count();
  DCHECK_NE(callback_id, 0u);

  // If the callback is different from the one the caller is already waiting on,
  // pass the callback through to SignalSyncToken. Otherwise the request is
  // redundant.
  if (callback_id != pending_callback_id) {
    // Use the compositor context because we want this callback on the
    // compositor thread.
    compositor_context_provider_->ContextSupport()->SignalSyncToken(
        latest_sync_token, std::move(callback));
  }

  return callback_id;
}

void GpuRasterBufferProvider::Shutdown() {}

void GpuRasterBufferProvider::RasterBufferImpl::PlaybackOnWorkerThread(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  RasterQuery query;
  query.depends_on_hardware_accelerated_jpeg_candidates =
      depends_on_hardware_accelerated_jpeg_candidates_;
  query.depends_on_hardware_accelerated_webp_candidates =
      depends_on_hardware_accelerated_webp_candidates_;
  PlaybackOnWorkerThreadInternal(raster_source, raster_full_rect,
                                 raster_dirty_rect, new_content_id, transform,
                                 playback_settings, url, &query);

  if (query.raster_duration_query_id) {
    if (query.raster_start_query_id)
      query.raster_buffer_creation_time = creation_time_;

    // Note that it is important to scope the raster context lock to
    // PlaybackOnWorkerThreadInternal and release it before calling this
    // function to avoid a deadlock in
    // RasterQueryQueue::CheckRasterFinishedQueries which acquires the raster
    // context lock while holding a lock used in the function.
    client_->pending_raster_queries_->Append(std::move(query));
  }
}

void GpuRasterBufferProvider::RasterBufferImpl::PlaybackOnWorkerThreadInternal(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url,
    RasterQuery* query) {
  gpu::raster::RasterInterface* ri =
      client_->worker_context_provider_->RasterInterface();
  DCHECK(ri);

  const bool measure_raster_metric = client_->metrics_subsampler_.ShouldSample(
      client_->raster_metric_probability_);

  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content_) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  if (measure_raster_metric) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Use a query to detect when the GPU side is ready to start issuing raster
    // work to the driver. We will use the resulting timestamp to measure raster
    // scheduling delay. We only care about this in Chrome OS because we will
    // use this timestamp to measure raster scheduling delay and we only need to
    // collect that data to assess the impact of hardware acceleration of image
    // decodes which work only in Chrome OS. Furthermore, we don't count raster
    // work that depends on at-raster image decodes. This is because we want the
    // delay to always include image decoding and uploading time, and at-raster
    // decodes should be relatively rare.
    if (!depends_on_at_raster_decodes_) {
      ri->GenQueriesEXT(1, &query->raster_start_query_id);
      DCHECK_GT(query->raster_start_query_id, 0u);
      ri->QueryCounterEXT(query->raster_start_query_id,
                          GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM);
    }
#else
    std::ignore = depends_on_at_raster_decodes_;
#endif

    // Use a query to time the GPU side work for rasterizing this tile.
    ri->GenQueriesEXT(1, &query->raster_duration_query_id);
    DCHECK_GT(query->raster_duration_query_id, 0u);
    ri->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM,
                      query->raster_duration_query_id);
  }

  {
    std::optional<base::ElapsedTimer> timer;
    if (measure_raster_metric)
      timer.emplace();
    RasterizeSource(raster_source, raster_full_rect, playback_rect, transform,
                    playback_settings);
    if (measure_raster_metric) {
      query->worker_raster_duration = timer->Elapsed();
      ri->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
    }
  }
}

void GpuRasterBufferProvider::RasterBufferImpl::RasterizeSource(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings) {
  gpu::raster::RasterInterface* ri =
      client_->worker_context_provider_->RasterInterface();
  bool mailbox_needs_clear = false;
  if (!backing_->shared_image) {
    DCHECK(!backing_->returned_sync_token.HasData());
    auto* sii = client_->worker_context_provider_->SharedImageInterface();

    // This SharedImage will serve as the destination of the raster defined by
    // `raster_source` before being sent off to the display compositor.
    gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    if (backing_->overlay_candidate) {
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      if (features::IsDelegatedCompositingEnabled())
        flags |= gpu::SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING;
    } else if (client_->is_using_raw_draw_) {
      flags |= gpu::SHARED_IMAGE_USAGE_RAW_DRAW;
    }
    backing_->shared_image =
        sii->CreateSharedImage({shared_image_format_, resource_size_,
                                color_space_, flags, "GpuRasterTile"},
                               gpu::kNullSurfaceHandle);
    CHECK(backing_->shared_image);
    mailbox_needs_clear = true;
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  } else {
    ri->WaitSyncTokenCHROMIUM(backing_->returned_sync_token.GetConstData());
  }

  // Assume legacy MSAA if sample count is positive.
  gpu::raster::MsaaMode msaa_mode =
      playback_settings.msaa_sample_count > 0
          ? (client_->is_using_dmsaa_ ? gpu::raster::kDMSAA
                                      : gpu::raster::kMSAA)
          : gpu::raster::kNoMSAA;
  // msaa_sample_count should be 1, 2, 4, 8, 16, 32, 64,
  // and log2(msaa_sample_count) should be [0,6].
  // If playback_settings.msaa_sample_count <= 0, the MSAA is not used. It is
  // equivalent to MSAA sample count 1.
  uint32_t sample_count =
      std::clamp(playback_settings.msaa_sample_count, 1, 64);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Gpu.Rasterization.Raster.MSAASampleCountLog2",
                              std::bit_width(sample_count) - 1, 0, 7, 7);
  // With Raw Draw, the framebuffer will be the rasterization target. It cannot
  // support LCD text, so disable LCD text for Raw Draw backings.
  // TODO(penghuang): remove it when sktext::gpu::Slug can be serialized.
  bool is_raw_draw_backing =
      client_->is_using_raw_draw_ && !backing_->overlay_candidate;
  bool use_lcd_text = playback_settings.use_lcd_text && !is_raw_draw_backing;

  ri->BeginRasterCHROMIUM(
      raster_source->background_color(), mailbox_needs_clear,
      playback_settings.msaa_sample_count, msaa_mode, use_lcd_text,
      playback_settings.visible, color_space_, playback_settings.hdr_headroom,
      backing_->shared_image->mailbox().name);

  gfx::Vector2dF recording_to_raster_scale = transform.scale();
  recording_to_raster_scale.InvScale(raster_source->recording_scale_factor());
  gfx::Size content_size = raster_source->GetContentSize(transform.scale());

  // TODO(enne): could skip the clear on new textures, as the service side has
  // to do that anyway.  resource_has_previous_content implies that the texture
  // is not new, but the reverse does not hold, so more plumbing is needed.
  ri->RasterCHROMIUM(
      raster_source->GetDisplayItemList().get(),
      playback_settings.image_provider, content_size, raster_full_rect,
      playback_rect, transform.translation(), recording_to_raster_scale,
      raster_source->requires_clear(),
      playback_settings.raster_inducing_scroll_offsets,
      const_cast<RasterSource*>(raster_source)->max_op_size_hint());
  ri->EndRasterCHROMIUM();

  // TODO(ericrk): Handle unpremultiply+dither for 4444 cases.
  // https://crbug.com/789153
}

bool GpuRasterBufferProvider::ShouldUnpremultiplyAndDitherResource(
    viz::SharedImageFormat format) const {
  // TODO(crbug.com/40042400): Re-enable for OOPR.
  return false;
}

}  // namespace cc
