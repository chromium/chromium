// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/one_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/debug/alias.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/gl/trace_util.h"

namespace cc {

OneCopyRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    OneCopyRasterBufferProvider* client,
    const ResourcePool::InUsePoolResource& in_use_resource,
    uint64_t previous_content_id)
    : client_(client),
      previous_content_id_(previous_content_id) {
  if (!in_use_resource.backing()) {
    auto backing = std::make_unique<ResourcePool::Backing>(
        in_use_resource.size(), in_use_resource.format(),
        in_use_resource.color_space());
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
  before_raster_sync_token_ = backing_->returned_sync_token;
  mailbox_texture_is_overlay_candidate_ = client_->tile_overlay_candidate_;
}

OneCopyRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  // This raster task is complete, so if the backing's SharedImage was created
  // on a worker thread during the raster work that has now happened.
  backing_->can_access_shared_image_on_compositor_thread = true;

  // This SyncToken was created on the worker context after uploading the
  // texture content.
  backing_->mailbox_sync_token = after_raster_sync_token_;
  if (after_raster_sync_token_.HasData()) {
    // The returned SyncToken was waited on in Playback. We know Playback
    // happened if the |after_raster_sync_token_| was set.
    backing_->returned_sync_token = gpu::SyncToken();
  }
  if (should_destroy_shared_image_ && backing_->shared_image()) {
    backing_->shared_image()->UpdateDestructionSyncToken(
        before_raster_sync_token_);
    backing_->clear_shared_image();
  }
}

void OneCopyRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  TRACE_EVENT0("cc", "OneCopyRasterBuffer::Playback");
  // The |before_raster_sync_token_| passed in here was created on the
  // compositor thread, or given back with the texture for reuse. This call
  // returns another SyncToken generated on the worker thread to synchronize
  // with after the raster is complete.
  after_raster_sync_token_ = client_->PlaybackAndCopyOnWorkerThread(
      backing_, mailbox_texture_is_overlay_candidate_,
      before_raster_sync_token_, raster_source, raster_full_rect,
      raster_dirty_rect, transform, playback_settings, previous_content_id_,
      new_content_id, should_destroy_shared_image_);
}

bool OneCopyRasterBufferProvider::RasterBufferImpl::
    SupportsBackgroundThreadPriority() const {
  // Playback() should not run at background thread priority because it acquires
  // the GpuChannelHost lock, which is acquired at normal thread priority by
  // other code. Acquiring it at background thread priority can cause a priority
  // inversion. https://crbug.com/1072756
  return false;
}

OneCopyRasterBufferProvider::OneCopyRasterBufferProvider(
    scoped_refptr<gpu::SharedImageInterface> sii,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    viz::RasterContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes,
    bool is_overlay_candidate)
    : sii_(sii),
      compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      use_partial_raster_(use_partial_raster),
      tile_overlay_candidate_(is_overlay_candidate),
      staging_pool_(std::move(task_runner),
                    worker_context_provider,
                    use_partial_raster,
                    max_staging_buffer_usage_in_bytes) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

OneCopyRasterBufferProvider::~OneCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
OneCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  // TODO(danakj): If resource_content_id != 0, we only need to copy/upload
  // the dirty rect.
  return std::make_unique<RasterBufferImpl>(this, resource,
                                            previous_content_id);
}

void OneCopyRasterBufferProvider::Flush() {
  // This flush on the compositor context flushes queued work on all contexts,
  // including the raster worker. Tile raster inserted a SyncToken which is
  // waited for in order to tell if a tile is ready for draw, but a flush
  // is needed to ensure the work is sent for those queries to get the right
  // answer.
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

bool OneCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  // While OneCopyRasterBufferProvider has an internal partial raster
  // implementation, it cannot directly partial raster into the externally
  // owned resource provided in AcquireBufferForRaster.
  return false;
}

bool OneCopyRasterBufferProvider::IsResourceReadyToDraw(
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

uint64_t OneCopyRasterBufferProvider::SetReadyToDrawCallback(
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

void OneCopyRasterBufferProvider::Shutdown() {
  staging_pool_.Shutdown();
}

gpu::SyncToken OneCopyRasterBufferProvider::PlaybackAndCopyOnWorkerThread(
    ResourcePool::Backing* backing,
    bool mailbox_texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id,
    bool& should_destroy_shared_image) {
  std::unique_ptr<StagingBuffer> staging_buffer =
      staging_pool_.AcquireStagingBuffer(backing->size(), backing->format(),
                                         previous_content_id);
  DCHECK(staging_buffer->size.width() >= raster_full_rect.width() &&
         staging_buffer->size.height() >= raster_full_rect.height());

  bool put_data_in_staging_buffer = PlaybackToStagingBuffer(
      staging_buffer.get(), raster_source, raster_full_rect, raster_dirty_rect,
      transform, backing->format(), backing->color_space(), playback_settings,
      previous_content_id, new_content_id);

  gpu::SyncToken sync_token_after_upload;

  if (put_data_in_staging_buffer) {
    sync_token_after_upload = CopyOnWorkerThread(
        staging_buffer.get(), raster_source, raster_full_rect, backing,
        mailbox_texture_is_overlay_candidate, sync_token);
  } else if (backing->shared_image()) {
    // If we failed to put data in the staging buffer
    // (https://crbug.com/554541), then we don't have anything to give to copy
    // into the resource. We report a zero mailbox that will result in
    // checkerboarding, and be treated as OOM which should retry.
    should_destroy_shared_image = true;
  }

  staging_pool_.ReleaseStagingBuffer(std::move(staging_buffer));
  return sync_token_after_upload;
}

bool OneCopyRasterBufferProvider::PlaybackToStagingBuffer(
    StagingBuffer* staging_buffer,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    const gfx::AxisTransform2d& transform,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& dst_color_space,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  gfx::Rect playback_rect = raster_full_rect;
  if (use_partial_raster_ && previous_content_id) {
    // Reduce playback rect to dirty region if the content id of the staging
    // buffer matches the previous content id.
    if (previous_content_id == staging_buffer->content_id) {
      playback_rect.Intersect(raster_dirty_rect);
    }
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  // Allocate mappable SharedImage if necessary.
  if (!staging_buffer->client_shared_image) {
    staging_buffer->client_shared_image = sii_->CreateSharedImage(
        {format, staging_buffer->size, dst_color_space,
         gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
             gpu::SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE,
         "OneCopyRasterStaging"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
    if (!staging_buffer->client_shared_image) {
      LOG(ERROR) << "Creation of StagingBuffer's SharedImage failed.";
      return false;
    }
  }

  auto mapping = staging_buffer->client_shared_image->Map();
  if (!mapping) {
    LOG(ERROR) << "MapSharedImage Failed.";
    return false;
  }
  staging_buffer->is_shared_memory = mapping->IsSharedMemory();

  RasterBufferProvider::PlaybackToMemory(
      mapping->GetMemoryForPlane(0).data(), format, staging_buffer->size,
      mapping->Stride(0), raster_source, raster_full_rect, playback_rect,
      transform, dst_color_space, playback_settings);

  staging_buffer->content_id = new_content_id;

  return true;
}

gpu::SyncToken OneCopyRasterBufferProvider::CopyOnWorkerThread(
    StagingBuffer* staging_buffer,
    const RasterSource* raster_source,
    const gfx::Rect& rect_to_copy,
    ResourcePool::Backing* backing,
    bool mailbox_texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token) {
  const gfx::Size& resource_size = backing->size();

  DCHECK(sii_);

  CHECK(staging_buffer->client_shared_image);

  bool needs_clear = false;

  if (!backing->shared_image()) {
    // This SharedImage will have the contents of raster operations copied into
    // it via the raster interface before being sent off to the display
    // compositor.
    gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    if (mailbox_texture_is_overlay_candidate)
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    backing->CreateSharedImage(sii_.get(), usage, "OneCopyRasterTile");
    // Clear the resource if we're not going to initialize it fully from the
    // copy due to non-exact resource reuse.  See https://crbug.com/1313091
    needs_clear = rect_to_copy.size() != resource_size;
  }

  sii_->UpdateSharedImage(staging_buffer->sync_token,
                          staging_buffer->client_shared_image->mailbox());

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);
  std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
      backing->shared_image()->BeginRasterAccess(ri, sync_token,
                                                 /*readonly=*/false);
  std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
      staging_buffer->client_shared_image->BeginRasterAccess(
          ri, sii_->GenUnverifiedSyncToken(), /*readonly=*/true);

  // Do not use queries unless COMMANDS_COMPLETED queries are supported, or
  // COMMANDS_ISSUED queries are sufficient.
  GLenum query_target = GL_NONE;

  if (worker_context_provider_->ContextCapabilities().sync_query) {
    // Use GL_COMMANDS_COMPLETED_CHROMIUM when supported because native
    // GpuMemoryBuffers can be accessed by the GPU after commands are issued
    // until GPU reads are done.
    query_target = GL_COMMANDS_COMPLETED_CHROMIUM;
  }

  // COMMANDS_ISSUED is sufficient for shared memory resources.
  if (staging_buffer->is_shared_memory) {
    query_target = GL_COMMANDS_ISSUED_CHROMIUM;
  }

  if (query_target != GL_NONE) {
    if (!staging_buffer->query_id)
      ri->GenQueriesEXT(1, &staging_buffer->query_id);

    ri->BeginQueryEXT(query_target, staging_buffer->query_id);
  }

  uint32_t texture_target = backing->shared_image()->GetTextureTarget();

  // Clear to ensure the resource is fully initialized and BeginAccess succeeds.
  if (needs_clear) {
    SkImageInfo dst_info = SkImageInfo::Make(
        {resource_size.width(), resource_size.height()},
        ToClosestSkColorType(backing->format()), kPremul_SkAlphaType);
    SkBitmap bitmap;
    if (bitmap.tryAllocPixels(dst_info)) {
      bitmap.eraseColor(raster_source->background_color());
      ri->WritePixels(backing->shared_image()->mailbox(), /*dst_x_offset=*/0,
                      /*dst_y_offset=*/0, texture_target, bitmap.pixmap());
    }
  }

  ri->CopySharedImage(staging_buffer->client_shared_image->mailbox(),
                      backing->shared_image()->mailbox(), 0, 0, 0, 0,
                      rect_to_copy.width(), rect_to_copy.height());

  if (query_target != GL_NONE)
    ri->EndQueryEXT(query_target);

  // Generate sync token on the worker context that will be sent to and waited
  // for by the display compositor before using the content generated here.
  // The same sync token is used to synchronize operations on the staging
  // buffer. Note, the query completion is generally enough to guarantee
  // ordering, but there are some paths (e.g.
  // StagingBufferPool::ReduceMemoryUsage) that may destroy the staging buffer
  // without waiting for the query completion.
  gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));
  gpu::SyncToken out_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(src_ri_access));
  staging_buffer->sync_token = out_sync_token;
  return out_sync_token;
}

}  // namespace cc
