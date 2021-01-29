// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
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
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "url/gurl.h"

namespace cc {
namespace {

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
    viz::RasterContextProvider* context_provider) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  if (mailbox->IsZero()) {
    DCHECK(!sync_token.HasData());
    auto* sii = context_provider->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                     gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    if (texture_is_overlay_candidate)
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    *mailbox = sii->CreateSharedImage(
        resource_format, resource_size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, flags, gpu::kNullSurfaceHandle);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  } else {
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

  ri->BeginRasterCHROMIUM(
      raster_source->background_color(), playback_settings.msaa_sample_count,
      playback_settings.use_lcd_text, color_space, mailbox->name);
  float recording_to_raster_scale =
      transform.scale() / raster_source->recording_scale_factor();
  gfx::Size content_size = raster_source->GetContentSize(transform.scale());

  // TODO(enne): could skip the clear on new textures, as the service side has
  // to do that anyway.  resource_has_previous_content implies that the texture
  // is not new, but the reverse does not hold, so more plumbing is needed.
  ri->RasterCHROMIUM(
      raster_source->GetDisplayItemList().get(),
      playback_settings.image_provider, content_size, raster_full_rect,
      playback_rect, transform.translation(), recording_to_raster_scale,
      raster_source->requires_clear(),
      const_cast<RasterSource*>(raster_source)->max_op_size_hint());
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
    const gfx::Size& max_tile_size) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  if (mailbox->IsZero()) {
    auto* sii = context_provider->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                     gpu::SHARED_IMAGE_USAGE_GLES2 |
                     gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
    if (texture_is_overlay_candidate)
      flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    *mailbox = sii->CreateSharedImage(
        resource_format, resource_size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, flags, gpu::kNullSurfaceHandle);
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  } else {
    // Wait on the SyncToken that was created on the compositor thread after
    // making the mailbox. This ensures that the mailbox we consume here is
    // valid by the time the consume command executes.
    ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
  GLuint texture_id = ri->CreateAndConsumeForGpuRaster(*mailbox);
  ri->BeginSharedImageAccessDirectCHROMIUM(
      texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  {
    ScopedGrContextAccess gr_context_access(context_provider);
    SkSurface* surface;
    sk_sp<SkColorSpace> sk_color_space = color_space.ToSkColorSpace();
    viz::ClientResourceProvider::ScopedSkSurface scoped_surface(
        context_provider->GrContext(), sk_color_space, texture_id,
        texture_target, resource_size, resource_format,
        skia::LegacyDisplayGlobals::ComputeSurfaceProps(
            playback_settings.use_lcd_text),
        playback_settings.msaa_sample_count);
    surface = scoped_surface.surface();

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
    raster_source->PlaybackToCanvas(canvas, content_size, raster_full_rect,
                                    playback_rect, transform,
                                    playback_settings);
  }
  ri->EndSharedImageAccessDirectCHROMIUM(texture_id);
  ri->DeleteGpuRasterTexture(texture_id);
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
    bool resource_has_previous_content,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates)
    : client_(client),
      backing_(backing),
      resource_size_(in_use_resource.size()),
      resource_format_(in_use_resource.format()),
      color_space_(in_use_resource.color_space()),
      resource_has_previous_content_(resource_has_previous_content),
      depends_on_at_raster_decodes_(depends_on_at_raster_decodes),
      depends_on_hardware_accelerated_jpeg_candidates_(
          depends_on_hardware_accelerated_jpeg_candidates),
      depends_on_hardware_accelerated_webp_candidates_(
          depends_on_hardware_accelerated_webp_candidates),
      before_raster_sync_token_(backing->returned_sync_token),
      texture_target_(backing->texture_target),
      texture_is_overlay_candidate_(backing->overlay_candidate),
      mailbox_(backing->mailbox) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only do this in Chrome OS with OOP-R because:
  //   1) We will use this timestamp to measure raster scheduling delay and we
  //      only need to collect that data to assess the impact of hardware
  //      acceleration of image decodes which works only on Chrome OS with
  //      OOP-R.
  //   2) We use CLOCK_MONOTONIC in that OS to get timestamps, so we can assert
  //      certain assumptions.
  if (client_->enable_oop_rasterization_)
    creation_time_ = base::TimeTicks::Now();
#endif
}

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
      raster_dirty_rect, new_content_id, transform, playback_settings, url,
      creation_time_, depends_on_at_raster_decodes_,
      depends_on_hardware_accelerated_jpeg_candidates_,
      depends_on_hardware_accelerated_webp_candidates_);
}

bool GpuRasterBufferProvider::RasterBufferImpl::
    SupportsBackgroundThreadPriority() const {
  return true;
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    bool use_gpu_memory_buffer_resources,
    viz::ResourceFormat tile_format,
    const gfx::Size& max_tile_size,
    bool unpremultiply_and_dither_low_bit_depth_tiles,
    bool enable_oop_rasterization,
    RasterQueryQueue* const pending_raster_queries,
    float raster_metric_probability)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      tile_format_(tile_format),
      max_tile_size_(max_tile_size),
      enable_oop_rasterization_(enable_oop_rasterization),
      pending_raster_queries_(pending_raster_queries),
      random_generator_(static_cast<uint32_t>(base::RandUint64())),
      bernoulli_distribution_(raster_metric_probability) {
  DCHECK(pending_raster_queries);
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
}

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
    backing->InitOverlayCandidateAndTextureTarget(
        resource.format(), compositor_context_provider_->ContextCapabilities(),
        use_gpu_memory_buffer_resources_);
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

viz::ResourceFormat GpuRasterBufferProvider::GetResourceFormat() const {
  return tile_format_;
}

bool GpuRasterBufferProvider::IsResourcePremultiplied() const {
  return !ShouldUnpremultiplyAndDitherResource(GetResourceFormat());
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

bool GpuRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
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
        latest_sync_token, std::move(callback));
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
    const GURL& url,
    base::TimeTicks raster_buffer_creation_time,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  RasterQuery query;
  query.depends_on_hardware_accelerated_jpeg_candidates =
      depends_on_hardware_accelerated_jpeg_candidates;
  query.depends_on_hardware_accelerated_webp_candidates =
      depends_on_hardware_accelerated_webp_candidates;
  gpu::SyncToken raster_finished_token = PlaybackOnWorkerThreadInternal(
      mailbox, texture_target, texture_is_overlay_candidate, sync_token,
      resource_size, resource_format, color_space,
      resource_has_previous_content, raster_source, raster_full_rect,
      raster_dirty_rect, new_content_id, transform, playback_settings, url,
      depends_on_at_raster_decodes, &query);

  if (query.raster_duration_query_id) {
    if (query.raster_start_query_id)
      query.raster_buffer_creation_time = raster_buffer_creation_time;

    // Note that it is important to scope the raster context lock to
    // PlaybackOnWorkerThreadInternal and release it before calling this
    // function to avoid a deadlock in
    // RasterQueryQueue::CheckRasterFinishedQueries which acquires the raster
    // context lock while holding a lock used in the function.
    pending_raster_queries_->Append(std::move(query));
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
    bool depends_on_at_raster_decodes,
    RasterQuery* query) {
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_, url.possibly_invalid_spec().c_str());
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);

  const bool measure_raster_metric = bernoulli_distribution_(random_generator_);

  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  if (measure_raster_metric) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Use a query to detect when the GPU side is ready to start issuing raster
    // work to the driver. We will use the resulting timestamp to measure raster
    // scheduling delay. We only care about this in Chrome OS and when OOP-R is
    // enabled because we will use this timestamp to measure raster scheduling
    // delay and we only need to collect that data to assess the impact of
    // hardware acceleration of image decodes which work only in Chrome OS with
    // OOP-R. Furthermore, we don't count raster work that depends on at-raster
    // image decodes. This is because we want the delay to always include
    // image decoding and uploading time, and at-raster decodes should be
    // relatively rare.
    if (enable_oop_rasterization_ && !depends_on_at_raster_decodes) {
      ri->GenQueriesEXT(1, &query->raster_start_query_id);
      DCHECK_GT(query->raster_start_query_id, 0u);
      ri->QueryCounterEXT(query->raster_start_query_id,
                          GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM);
    }
#endif

    // Use a query to time the GPU side work for rasterizing this tile.
    ri->GenQueriesEXT(1, &query->raster_duration_query_id);
    DCHECK_GT(query->raster_duration_query_id, 0u);
    ri->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM,
                      query->raster_duration_query_id);
  }

  {
    base::Optional<base::ElapsedTimer> timer;
    if (measure_raster_metric)
      timer.emplace();
    if (enable_oop_rasterization_) {
      RasterizeSourceOOP(
          raster_source, resource_has_previous_content, mailbox, sync_token,
          texture_target, texture_is_overlay_candidate, resource_size,
          resource_format, color_space, raster_full_rect, playback_rect,
          transform, playback_settings, worker_context_provider_);
    } else {
      RasterizeSource(raster_source, resource_has_previous_content, mailbox,
                      sync_token, texture_target, texture_is_overlay_candidate,
                      resource_size, resource_format, color_space,
                      raster_full_rect, playback_rect, transform,
                      playback_settings, worker_context_provider_,
                      max_tile_size_);
    }
    if (measure_raster_metric) {
      query->worker_raster_duration = timer->Elapsed();
      ri->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
    }
  }

  // Generate sync token for cross context synchronization.
  return viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
}

bool GpuRasterBufferProvider::ShouldUnpremultiplyAndDitherResource(
    viz::ResourceFormat format) const {
  // TODO(crbug.com/1151490): Re-enable for OOPR.
  return false;
}

}  // namespace cc
