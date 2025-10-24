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
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "cc/raster/raster_source.h"
#include "components/viz/client/client_resource_provider.h"
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

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    const ResourcePool::InUsePoolResource& in_use_resource,
    bool resource_has_previous_content)
    : client_(client),
      resource_has_previous_content_(resource_has_previous_content) {
  if (!in_use_resource.backing()) {
    auto backing = std::make_unique<ResourcePool::Backing>(
        in_use_resource.size(), in_use_resource.format(),
        in_use_resource.color_space());
    backing->is_using_raw_draw =
        !client_->tile_overlay_candidate_ && client_->is_using_raw_draw_;
    in_use_resource.set_backing(std::move(backing));
  }
  backing_ = in_use_resource.backing();
  if (!backing_->shared_image()) {
    // The backing's SharedImage will be created on a worker thread during the
    // execution of this raster; to avoid data races during taking of memory
    // dumps on the compositor thread, mark the backing's SharedImage as
    // unavailable for access on the compositor thread for the duration of the
    // raster.
    backing_->can_access_shared_image_on_compositor_thread = false;
  }
}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  // This raster task is complete, so if the backing's SharedImage was created
  // on a worker thread during the raster work that has now happened.
  backing_->can_access_shared_image_on_compositor_thread = true;
}

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");
  PlaybackOnWorkerThread(raster_source, raster_full_rect, raster_dirty_rect,
                         new_content_id, transform, playback_settings, url);

  backing_->returned_sync_token = gpu::SyncToken();
}

bool GpuRasterBufferProvider::RasterBufferImpl::
    SupportsBackgroundThreadPriority() const {
  return true;
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    scoped_refptr<gpu::SharedImageInterface> sii,
    viz::RasterContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    bool is_overlay_candidate,
    const gfx::Size& max_tile_size,
    RasterQueryQueue* const pending_raster_queries,
    float raster_metric_probability)
    : sii_(sii),
      compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      tile_overlay_candidate_(is_overlay_candidate),
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
    // kUseDMSAAForTiles.
    is_using_dmsaa_ = !is_using_vulkan ||
                      base::FeatureList::IsEnabled(features::kUseDMSAAForTiles);
  }
#endif
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<RasterBufferImpl>(this, resource,
                                            resource_has_previous_content);
}

void GpuRasterBufferProvider::Flush() {
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

bool GpuRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) {
  FlushIfNeeded();
  const gpu::SyncToken& sync_token = resource.backing()->mailbox_sync_token;
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
    const gpu::SyncToken& sync_token = in_use->backing()->mailbox_sync_token;
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
  PlaybackOnWorkerThreadInternal(raster_source, raster_full_rect,
                                 raster_dirty_rect, new_content_id, transform,
                                 playback_settings, url, &query);

  if (query.raster_duration_query_id) {
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
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      client_->worker_context_provider_, url.possibly_invalid_spec().c_str());
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
  std::unique_ptr<gpu::RasterScopedAccess> ri_access;
  if (!backing_->shared_image()) {
    DCHECK(!backing_->returned_sync_token.HasData());
    auto* sii = client_->sii_.get();

    // This SharedImage will serve as the destination of the raster defined by
    // `raster_source` before being sent off to the display compositor.
    gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    if (client_->tile_overlay_candidate_) {
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    } else if (client_->is_using_raw_draw_) {
      flags |= gpu::SHARED_IMAGE_USAGE_RAW_DRAW;
    }
    backing_->CreateSharedImage(sii, flags, "GpuRasterTile");
    mailbox_needs_clear = true;
    ri_access = backing_->shared_image()->BeginRasterAccess(
        ri, sii->GenUnverifiedSyncToken(),
        /*readonly=*/false);
  } else {
    ri_access = backing_->shared_image()->BeginRasterAccess(
        ri, backing_->returned_sync_token,
        /*readonly=*/false);
  }

  // Assume legacy MSAA if sample count is positive.
  gpu::raster::MsaaMode msaa_mode =
      playback_settings.msaa_sample_count > 0
          ? (client_->is_using_dmsaa_ ? gpu::raster::kDMSAA
                                      : gpu::raster::kMSAA)
          : gpu::raster::kNoMSAA;
  // With Raw Draw, the framebuffer will be the rasterization target. It cannot
  // support LCD text, so disable LCD text for Raw Draw backings.
  // TODO(penghuang): remove it when sktext::gpu::Slug can be serialized.
  bool is_raw_draw_backing =
      client_->is_using_raw_draw_ && !client_->tile_overlay_candidate_;
  bool use_lcd_text = playback_settings.use_lcd_text && !is_raw_draw_backing;

  ri->BeginRasterCHROMIUM(
      raster_source->background_color(), mailbox_needs_clear,
      playback_settings.msaa_sample_count, msaa_mode, use_lcd_text,
      playback_settings.visible, backing_->color_space(),
      playback_settings.hdr_headroom, backing_->shared_image()->mailbox().name);

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
  backing_->mailbox_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(ri_access));
}

}  // namespace cc
