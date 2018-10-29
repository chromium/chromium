// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/histograms.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/raster/scoped_grcontext_access.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/gpu/texture_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "url/gurl.h"

namespace cc {
namespace {

class ScopedSkSurfaceForUnpremultiplyAndDither {
 public:
  ScopedSkSurfaceForUnpremultiplyAndDither(
      viz::RasterContextProvider* context_provider,
      const gfx::Rect& playback_rect,
      const gfx::Rect& raster_full_rect,
      const gfx::Size& max_tile_size,
      GLuint texture_id,
      const gfx::Size& texture_size,
      bool can_use_lcd_text,
      int msaa_sample_count)
      : context_provider_(context_provider),
        texture_id_(texture_id),
        offset_(playback_rect.OffsetFromOrigin() -
                raster_full_rect.OffsetFromOrigin()),
        size_(playback_rect.size()) {
    // Determine the |intermediate_size| to use for our 32-bit texture. If we
    // know the max tile size, use that. This prevents GPU cache explosion due
    // to using lots of different 32-bit texture sizes. Otherwise just use the
    // exact size of the target texture.
    gfx::Size intermediate_size;
    if (!max_tile_size.IsEmpty()) {
      DCHECK_GE(max_tile_size.width(), texture_size.width());
      DCHECK_GE(max_tile_size.height(), texture_size.height());
      intermediate_size = max_tile_size;
    } else {
      intermediate_size = texture_size;
    }

    // Allocate a 32-bit surface for raster. We will copy from that into our
    // actual surface in destruction.
    SkImageInfo n32Info = SkImageInfo::MakeN32Premul(
        intermediate_size.width(), intermediate_size.height());
    SkSurfaceProps surface_props =
        viz::ClientResourceProvider::ScopedSkSurface::ComputeSurfaceProps(
            can_use_lcd_text);
    surface_ = SkSurface::MakeRenderTarget(
        context_provider->GrContext(), SkBudgeted::kNo, n32Info,
        msaa_sample_count, kTopLeft_GrSurfaceOrigin, &surface_props);
  }

  ~ScopedSkSurfaceForUnpremultiplyAndDither() {
    // In lost-context cases, |surface_| may be null and there's nothing
    // meaningful to do here.
    if (!surface_)
      return;

    GrBackendTexture backend_texture =
        surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
    if (!backend_texture.isValid()) {
      return;
    }
    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info)) {
      return;
    }
    context_provider_->ContextGL()->UnpremultiplyAndDitherCopyCHROMIUM(
        info.fID, texture_id_, offset_.x(), offset_.y(), size_.width(),
        size_.height());
  }

  SkSurface* surface() { return surface_.get(); }

 private:
  viz::RasterContextProvider* context_provider_;
  GLuint texture_id_;
  gfx::Vector2d offset_;
  gfx::Size size_;
  sk_sp<SkSurface> surface_;
};

static void RasterizeSourceOOP(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    gpu::Mailbox* mailbox,
    const gpu::SyncToken& sync_token,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    int msaa_sample_count) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  if (mailbox->IsZero()) {
    DCHECK(!sync_token.HasData());
    auto* sii = context_provider->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    if (texture_is_overlay_candidate)
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    *mailbox = sii->CreateSharedImage(resource_format, resource_size,
                                      color_space, flags);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  } else {
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

  // TODO(enne): Use the |texture_target|? GpuMemoryBuffer backed textures don't
  // use GL_TEXTURE_2D.
  ri->BeginRasterCHROMIUM(raster_source->background_color(), msaa_sample_count,
                          playback_settings.use_lcd_text,
                          viz::ResourceFormatToClosestSkColorType(
                              /*gpu_compositing=*/true, resource_format),
                          playback_settings.raster_color_space, mailbox->name);
  float recording_to_raster_scale =
      transform.scale() / raster_source->recording_scale_factor();
  gfx::Size content_size = raster_source->GetContentSize(transform.scale());

  // TODO(enne): could skip the clear on new textures, as the service side has
  // to do that anyway.  resource_has_previous_content implies that the texture
  // is not new, but the reverse does not hold, so more plumbing is needed.
  ri->RasterCHROMIUM(raster_source->GetDisplayItemList().get(),
                     playback_settings.image_provider, content_size,
                     raster_full_rect, playback_rect, transform.translation(),
                     recording_to_raster_scale,
                     raster_source->requires_clear());
  ri->EndRasterCHROMIUM();

  // TODO(ericrk): Handle unpremultiply+dither for 4444 cases.
  // https://crbug.com/789153
}

static void RasterizeSource(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    gpu::Mailbox* mailbox,
    const gpu::SyncToken& sync_token,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    int msaa_sample_count,
    bool unpremultiply_and_dither,
    const gfx::Size& max_tile_size) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  if (mailbox->IsZero()) {
    auto* sii = context_provider->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_GLES2 |
                     gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
    if (texture_is_overlay_candidate)
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    *mailbox = sii->CreateSharedImage(resource_format, resource_size,
                                      color_space, flags);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  } else {
    // Wait on the SyncToken that was created on the compositor thread after
    // making the mailbox. This ensures that the mailbox we consume here is
    // valid by the time the consume command executes.
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
  GLuint texture_id = ri->CreateAndConsumeTexture(
      texture_is_overlay_candidate, gfx::BufferUsage::SCANOUT, resource_format,
      mailbox->name);
  {
    ScopedGrContextAccess gr_context_access(context_provider);
    base::Optional<viz::ClientResourceProvider::ScopedSkSurface> scoped_surface;
    base::Optional<ScopedSkSurfaceForUnpremultiplyAndDither>
        scoped_dither_surface;
    SkSurface* surface;
    if (!unpremultiply_and_dither) {
      scoped_surface.emplace(context_provider->GrContext(), texture_id,
                             texture_target, resource_size, resource_format,
                             playback_settings.use_lcd_text, msaa_sample_count);
      surface = scoped_surface->surface();
    } else {
      scoped_dither_surface.emplace(
          context_provider, playback_rect, raster_full_rect, max_tile_size,
          texture_id, resource_size, playback_settings.use_lcd_text,
          msaa_sample_count);
      surface = scoped_dither_surface->surface();
    }

    // Allocating an SkSurface will fail after a lost context.  Pretend we
    // rasterized, as the contents of the resource don't matter anymore.
    if (!surface) {
      DLOG(ERROR) << "Failed to allocate raster surface";
      return;
    }

    SkCanvas* canvas = surface->getCanvas();

    // As an optimization, inform Skia to discard when not doing partial raster.
    if (raster_full_rect == playback_rect)
      canvas->discard();

    gfx::Size content_size = raster_source->GetContentSize(transform.scale());
    raster_source->PlaybackToCanvas(canvas, color_space, content_size,
                                    raster_full_rect, playback_rect, transform,
                                    playback_settings);
  }

  ri->DeleteTextures(1, &texture_id);
}

}  // namespace

// Subclass for InUsePoolResource that holds ownership of a gpu-rastered backing
// and does cleanup of the backing when destroyed.
class GpuRasterBufferProvider::GpuRasterBacking
    : public ResourcePool::GpuBacking {
 public:
  ~GpuRasterBacking() override {
    if (mailbox.IsZero())
      return;
    auto* sii = worker_context_provider->SharedImageInterface();
    if (returned_sync_token.HasData())
      sii->DestroySharedImage(returned_sync_token, mailbox);
    else if (mailbox_sync_token.HasData())
      sii->DestroySharedImage(mailbox_sync_token, mailbox);
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

  // The ContextProvider used to clean up the mailbox
  viz::RasterContextProvider* worker_context_provider = nullptr;
};

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    const ResourcePool::InUsePoolResource& in_use_resource,
    GpuRasterBacking* backing,
    bool resource_has_previous_content)
    : client_(client),
      backing_(backing),
      resource_size_(in_use_resource.size()),
      resource_format_(in_use_resource.format()),
      color_space_(in_use_resource.color_space()),
      resource_has_previous_content_(resource_has_previous_content),
      before_raster_sync_token_(backing->returned_sync_token),
      texture_target_(backing->texture_target),
      texture_is_overlay_candidate_(backing->overlay_candidate),
      mailbox_(backing->mailbox) {}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  // This SyncToken was created on the worker context after rastering the
  // texture content.
  backing_->mailbox_sync_token = after_raster_sync_token_;
  if (after_raster_sync_token_.HasData()) {
    // The returned SyncToken was waited on in Playback. We know Playback
    // happened if the |after_raster_sync_token_| was set.
    backing_->returned_sync_token = gpu::SyncToken();
  }
  backing_->mailbox = mailbox_;
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
  // The |before_raster_sync_token_| passed in here was created on the
  // compositor thread, or given back with the texture for reuse. This call
  // returns another SyncToken generated on the worker thread to synchronize
  // with after the raster is complete.
  after_raster_sync_token_ = client_->PlaybackOnWorkerThread(
      &mailbox_, texture_target_, texture_is_overlay_candidate_,
      before_raster_sync_token_, resource_size_, resource_format_, color_space_,
      resource_has_previous_content_, raster_source, raster_full_rect,
      raster_dirty_rect, new_content_id, transform, playback_settings, url);
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    bool use_gpu_memory_buffer_resources,
    int gpu_rasterization_msaa_sample_count,
    viz::ResourceFormat tile_format,
    const gfx::Size& max_tile_size,
    bool unpremultiply_and_dither_low_bit_depth_tiles,
    bool enable_oop_rasterization,
    int raster_metric_frequency)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      msaa_sample_count_(gpu_rasterization_msaa_sample_count),
      tile_format_(tile_format),
      max_tile_size_(max_tile_size),
      unpremultiply_and_dither_low_bit_depth_tiles_(
          unpremultiply_and_dither_low_bit_depth_tiles),
      enable_oop_rasterization_(enable_oop_rasterization),
      raster_metric_frequency_(raster_metric_frequency) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
}

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<GpuRasterBacking>();
    backing->worker_context_provider = worker_context_provider_;
    backing->InitOverlayCandidateAndTextureTarget(
        resource.format(), compositor_context_provider_->ContextCapabilities(),
        use_gpu_memory_buffer_resources_);
    resource.set_gpu_backing(std::move(backing));
  }
  GpuRasterBacking* backing =
      static_cast<GpuRasterBacking*>(resource.gpu_backing());
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<RasterBufferImpl>(this, resource, backing,
                                            resource_has_previous_content);
}

void GpuRasterBufferProvider::Flush() {
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

viz::ResourceFormat GpuRasterBufferProvider::GetResourceFormat() const {
  return tile_format_;
}

bool GpuRasterBufferProvider::IsResourceSwizzleRequired() const {
  // This doesn't require a swizzle because we rasterize to the correct format.
  return false;
}

bool GpuRasterBufferProvider::IsResourcePremultiplied() const {
  return !ShouldUnpremultiplyAndDitherResource(GetResourceFormat());
}

bool GpuRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  // Partial raster doesn't support MSAA, as the MSAA resolve is unaware of clip
  // rects.
  // TODO(crbug.com/629683): See if we can work around this limitation.
  return msaa_sample_count_ == 0;
}

bool GpuRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  const gpu::SyncToken& sync_token = resource.gpu_backing()->mailbox_sync_token;
  // This SyncToken() should have been set by calling OrderingBarrier() before
  // calling this.
  DCHECK(sync_token.HasData());

  // IsSyncTokenSignaled is thread-safe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignaled(
      sync_token);
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
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
        latest_sync_token, callback);
  }

  return callback_id;
}

void GpuRasterBufferProvider::Shutdown() {
}

gpu::SyncToken GpuRasterBufferProvider::PlaybackOnWorkerThread(
    gpu::Mailbox* mailbox,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    bool resource_has_previous_content,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  PendingRasterQuery query;
  gpu::SyncToken raster_finished_token = PlaybackOnWorkerThreadInternal(
      mailbox, texture_target, texture_is_overlay_candidate, sync_token,
      resource_size, resource_format, color_space,
      resource_has_previous_content, raster_source, raster_full_rect,
      raster_dirty_rect, new_content_id, transform, playback_settings, url,
      &query);

  if (query.query_id != 0u) {
    // Note that it is important to scope the raster context lock to
    // PlaybackOnWorkerThreadInternal and release it before acquiring this lock
    // to avoid a deadlock in CheckRasterFinishedQueries which acquires the
    // raster context lock while holding this lock.
    base::AutoLock hold(pending_raster_queries_lock_);
    pending_raster_queries_.push_back(query);
  }

  return raster_finished_token;
}

gpu::SyncToken GpuRasterBufferProvider::PlaybackOnWorkerThreadInternal(
    gpu::Mailbox* mailbox,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    bool resource_has_previous_content,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url,
    PendingRasterQuery* query) {
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_, url.possibly_invalid_spec().c_str());
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);

  raster_tasks_count_++;
  bool measure_raster_metric = false;
  if (raster_tasks_count_ == raster_metric_frequency_) {
    measure_raster_metric = true;
    raster_tasks_count_ = 0;
  }

  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  // Log a histogram of the percentage of pixels that were saved due to
  // partial raster.
  const char* client_name = GetClientNameForMetrics();
  float full_rect_size = raster_full_rect.size().GetArea();
  if (full_rect_size > 0 && client_name) {
    float fraction_partial_rastered =
        static_cast<float>(playback_rect.size().GetArea()) / full_rect_size;
    float fraction_saved = 1.0f - fraction_partial_rastered;
    UMA_HISTOGRAM_PERCENTAGE(
        base::StringPrintf("Renderer4.%s.PartialRasterPercentageSaved.Gpu",
                           client_name),
        100.0f * fraction_saved);
  }

  if (measure_raster_metric) {
    // Use a query to time the GPU side work for rasterizing this tile.
    ri->GenQueriesEXT(1, &query->query_id);
    ri->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query->query_id);
  }

  {
    base::Optional<base::ElapsedTimer> timer;
    if (measure_raster_metric)
      timer.emplace();
    if (enable_oop_rasterization_) {
      RasterizeSourceOOP(raster_source, resource_has_previous_content, mailbox,
                         sync_token, texture_target,
                         texture_is_overlay_candidate, resource_size,
                         resource_format, color_space, raster_full_rect,
                         playback_rect, transform, playback_settings,
                         worker_context_provider_, msaa_sample_count_);
    } else {
      RasterizeSource(raster_source, resource_has_previous_content, mailbox,
                      sync_token, texture_target, texture_is_overlay_candidate,
                      resource_size, resource_format, color_space,
                      raster_full_rect, playback_rect, transform,
                      playback_settings, worker_context_provider_,
                      msaa_sample_count_,
                      ShouldUnpremultiplyAndDitherResource(resource_format),
                      max_tile_size_);
    }
    if (measure_raster_metric)
      query->worker_duration = timer->Elapsed();
  }

  ri->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);

  // Generate sync token for cross context synchronization.
  return viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
}

bool GpuRasterBufferProvider::ShouldUnpremultiplyAndDitherResource(
    viz::ResourceFormat format) const {
  switch (format) {
    case viz::RGBA_4444:
      return unpremultiply_and_dither_low_bit_depth_tiles_;
    default:
      return false;
  }
}

#define UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(name, total_time) \
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                              \
      name, total_time, base::TimeDelta::FromMicroseconds(1),           \
      base::TimeDelta::FromMilliseconds(100), 100);

bool GpuRasterBufferProvider::CheckRasterFinishedQueries() {
  base::AutoLock hold(pending_raster_queries_lock_);
  if (pending_raster_queries_.empty())
    return false;

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  auto* ri = scoped_context.RasterInterface();

  auto it = pending_raster_queries_.begin();
  while (it != pending_raster_queries_.end()) {
    GLuint complete = 1;
    ri->GetQueryObjectuivEXT(it->query_id,
                             GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT,
                             &complete);
    if (!complete)
      break;

    GLuint gpu_duration = 0u;
    ri->GetQueryObjectuivEXT(it->query_id, GL_QUERY_RESULT_EXT, &gpu_duration);
    ri->DeleteQueriesEXT(1, &it->query_id);

    base::TimeDelta total_time =
        it->worker_duration + base::TimeDelta::FromMicroseconds(gpu_duration);

    // It is safe to use the UMA macros here with runtime generated strings
    // because the client name should be initialized once in the process, before
    // recording any metrics here.
    const char* client_name = GetClientNameForMetrics();
    if (enable_oop_rasterization_) {
      UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
          base::StringPrintf("Renderer4.%s.RasterTaskTotalDuration.Oop",
                             client_name),
          total_time);
    } else {
      UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
          base::StringPrintf("Renderer4.%s.RasterTaskTotalDuration.Gpu",
                             client_name),
          total_time);
    }

    it = pending_raster_queries_.erase(it);
  }

  return pending_raster_queries_.size() > 0u;
}

}  // namespace cc
