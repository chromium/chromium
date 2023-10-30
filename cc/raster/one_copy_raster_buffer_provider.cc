// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/one_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {

// 4MiB is the size of 4 512x512 tiles, which has proven to be a good
// default batch size for copy operations.
const int kMaxBytesPerCopyOperation = 1024 * 1024 * 4;

BASE_FEATURE(kAlwaysUseMappableSIForOneCopyRaster,
             "AlwaysUseMappableSIForOneCopyRaster",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// Subclass for InUsePoolResource that holds ownership of a one-copy backing
// and does cleanup of the backing when destroyed.
class OneCopyRasterBufferProvider::OneCopyGpuBacking
    : public ResourcePool::GpuBacking {
 public:
  ~OneCopyGpuBacking() override {
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

  // The context used to clean up the mailbox
  raw_ptr<viz::RasterContextProvider> worker_context_provider = nullptr;
};

OneCopyRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    OneCopyRasterBufferProvider* client,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const ResourcePool::InUsePoolResource& in_use_resource,
    OneCopyGpuBacking* backing,
    uint64_t previous_content_id)
    : client_(client),
      backing_(backing),
      resource_size_(in_use_resource.size()),
      format_(in_use_resource.format()),
      color_space_(in_use_resource.color_space()),
      previous_content_id_(previous_content_id),
      before_raster_sync_token_(backing->returned_sync_token),
      mailbox_(backing->mailbox),
      mailbox_texture_target_(backing->texture_target),
      mailbox_texture_is_overlay_candidate_(backing->overlay_candidate) {}

OneCopyRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  // This SyncToken was created on the worker context after uploading the
  // texture content.
  backing_->mailbox_sync_token = after_raster_sync_token_;
  if (after_raster_sync_token_.HasData()) {
    // The returned SyncToken was waited on in Playback. We know Playback
    // happened if the |after_raster_sync_token_| was set.
    backing_->returned_sync_token = gpu::SyncToken();
  }
  backing_->mailbox = mailbox_;
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
      &mailbox_, mailbox_texture_target_, mailbox_texture_is_overlay_candidate_,
      before_raster_sync_token_, raster_source, raster_full_rect,
      raster_dirty_rect, transform, resource_size_, format_, color_space_,
      playback_settings, previous_content_id_, new_content_id);
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
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    viz::RasterContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int max_copy_texture_chromium_size,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes,
    const RasterCapabilities& raster_caps)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      max_bytes_per_copy_operation_(
          max_copy_texture_chromium_size
              ? std::min(kMaxBytesPerCopyOperation,
                         max_copy_texture_chromium_size)
              : kMaxBytesPerCopyOperation),
      use_partial_raster_(use_partial_raster),
      bytes_scheduled_since_last_flush_(0),
      tile_format_(raster_caps.tile_format),
      tile_overlay_candidate_(raster_caps.tile_overlay_candidate),
      tile_texture_target_(raster_caps.tile_texture_target),
      staging_pool_(std::move(task_runner),
                    worker_context_provider,
                    use_partial_raster,
                    max_staging_buffer_usage_in_bytes) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
  DCHECK(!tile_format_.IsCompressed());
}

OneCopyRasterBufferProvider::~OneCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
OneCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<OneCopyGpuBacking>();
    backing->worker_context_provider = worker_context_provider_;
    backing->overlay_candidate = tile_overlay_candidate_;
    backing->texture_target = tile_texture_target_;
    resource.set_gpu_backing(std::move(backing));
  }
  OneCopyGpuBacking* backing =
      static_cast<OneCopyGpuBacking*>(resource.gpu_backing());
  // TODO(danakj): If resource_content_id != 0, we only need to copy/upload
  // the dirty rect.
  return std::make_unique<RasterBufferImpl>(
      this, gpu_memory_buffer_manager_, resource, backing, previous_content_id);
}

void OneCopyRasterBufferProvider::Flush() {
  // This flush on the compositor context flushes queued work on all contexts,
  // including the raster worker. Tile raster inserted a SyncToken which is
  // waited for in order to tell if a tile is ready for draw, but a flush
  // is needed to ensure the work is sent for those queries to get the right
  // answer.
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

viz::SharedImageFormat OneCopyRasterBufferProvider::GetFormat() const {
  return tile_format_;
}

bool OneCopyRasterBufferProvider::IsResourcePremultiplied() const {
  // TODO(ericrk): Handle unpremultiply/dither in one-copy case as well.
  // https://crbug.com/789153
  return true;
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
  const gpu::SyncToken& sync_token = resource.gpu_backing()->mailbox_sync_token;
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

void OneCopyRasterBufferProvider::SetShutdownEvent(
    base::WaitableEvent* shutdown_event) {
  shutdown_event_ = shutdown_event;
}

void OneCopyRasterBufferProvider::Shutdown() {
  staging_pool_.Shutdown();
}

gpu::SyncToken OneCopyRasterBufferProvider::PlaybackAndCopyOnWorkerThread(
    gpu::Mailbox* mailbox,
    GLenum mailbox_texture_target,
    bool mailbox_texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    const gfx::AxisTransform2d& transform,
    const gfx::Size& resource_size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  std::unique_ptr<StagingBuffer> staging_buffer =
      staging_pool_.AcquireStagingBuffer(resource_size, format,
                                         previous_content_id);
  DCHECK(staging_buffer->size.width() >= raster_full_rect.width() &&
         staging_buffer->size.height() >= raster_full_rect.height());

  bool put_data_in_staging_buffer = PlaybackToStagingBuffer(
      staging_buffer.get(), raster_source, raster_full_rect, raster_dirty_rect,
      transform, format, color_space, playback_settings, previous_content_id,
      new_content_id);

  gpu::SyncToken sync_token_after_upload;

  if (put_data_in_staging_buffer) {
    sync_token_after_upload = CopyOnWorkerThread(
        staging_buffer.get(), raster_source, raster_full_rect, format,
        resource_size, mailbox, mailbox_texture_target,
        mailbox_texture_is_overlay_candidate, sync_token, color_space);
  } else {
    // If we failed to put data in the staging buffer
    // (https://crbug.com/554541), then we don't have anything to give to copy
    // into the resource. We report a zero mailbox that will result in
    // checkerboarding, and be treated as OOM which should retry.
    if (!mailbox->IsZero()) {
      worker_context_provider_->SharedImageInterface()->DestroySharedImage(
          sync_token, *mailbox);
      mailbox->SetZero();
    }
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
  std::unique_ptr<gpu::SharedImageInterface::ScopedMapping> mapping;
  gfx::GpuMemoryBuffer* buffer = nullptr;
  void* memory = nullptr;
  size_t stride = 0;

  gfx::Rect playback_rect = raster_full_rect;
  if (use_partial_raster_ && previous_content_id) {
    // Reduce playback rect to dirty region if the content id of the staging
    // buffer matches the previous content id.
    if (previous_content_id == staging_buffer->content_id) {
      playback_rect.Intersect(raster_dirty_rect);
    }
  }

  float full_rect_size = raster_full_rect.size().GetArea();

  if (base::FeatureList::IsEnabled(kAlwaysUseMappableSIForOneCopyRaster)) {
    CHECK(!staging_buffer->gpu_memory_buffer);

    auto* sii = worker_context_provider_->SharedImageInterface();

    // Allocate MappableSharedImage if necessary.
    if (staging_buffer->mailbox.IsZero()) {
      auto client_shared_image = sii->CreateSharedImage(
          format, staging_buffer->size, dst_color_space,
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          gpu::SHARED_IMAGE_USAGE_CPU_WRITE, "OneCopyRasterStaging",
          gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
      if (!client_shared_image) {
        LOG(ERROR) << "Creation of MappableSharedImage failed.";
        return false;
      }
      staging_buffer->mailbox = client_shared_image->mailbox();
    }

    mapping = sii->MapSharedImage(staging_buffer->mailbox);
    if (!mapping) {
      LOG(ERROR) << "MapSharedImage Failed.";
      return false;
    }
    memory = mapping->Memory(0);
    stride = mapping->Stride(0);
    staging_buffer->is_shared_memory = mapping->IsSharedMemory();
  } else {
    // Allocate GpuMemoryBuffer if necessary.
    if (!staging_buffer->gpu_memory_buffer) {
      staging_buffer->gpu_memory_buffer =
          gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
              staging_buffer->size,
              viz::SinglePlaneSharedImageFormatToBufferFormat(format),
              gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
              gpu::kNullSurfaceHandle, shutdown_event_);
    }

    buffer = staging_buffer->gpu_memory_buffer.get();
    if (!buffer) {
      return false;
    }

    CHECK_EQ(1u, gfx::NumberOfPlanesForLinearBufferFormat(buffer->GetFormat()));
    bool rv = buffer->Map();
    CHECK(rv);
    CHECK(buffer->memory(0));
    // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
    CHECK_GE(buffer->stride(0), 0);

    // TODO(https://crbug.com/870663): Temporary diagnostics.
    base::debug::Alias(&playback_rect);
    base::debug::Alias(&full_rect_size);
    base::debug::Alias(&rv);
    void* buffer_memory = buffer->memory(0);
    base::debug::Alias(&buffer_memory);
    gfx::Size staging_buffer_size = staging_buffer->size;
    base::debug::Alias(&staging_buffer_size);
    gfx::Size buffer_size = buffer->GetSize();
    base::debug::Alias(&buffer_size);

    memory = buffer->memory(0);
    stride = buffer->stride(0);
    staging_buffer->is_shared_memory =
        buffer->GetType() == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
  }

  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";
  RasterBufferProvider::PlaybackToMemory(
      memory, format, staging_buffer->size, stride, raster_source,
      raster_full_rect, playback_rect, transform, dst_color_space,
      /*gpu_compositing=*/true, playback_settings);
  base::FeatureList::IsEnabled(kAlwaysUseMappableSIForOneCopyRaster)
      ? mapping.reset()
      : buffer->Unmap();
  staging_buffer->content_id = new_content_id;

  return true;
}

gpu::SyncToken OneCopyRasterBufferProvider::CopyOnWorkerThread(
    StagingBuffer* staging_buffer,
    const RasterSource* raster_source,
    const gfx::Rect& rect_to_copy,
    viz::SharedImageFormat format,
    const gfx::Size& resource_size,
    gpu::Mailbox* mailbox,
    GLenum mailbox_texture_target,
    bool mailbox_texture_is_overlay_candidate,
    const gpu::SyncToken& sync_token,
    const gfx::ColorSpace& color_space) {
  auto* sii = worker_context_provider_->SharedImageInterface();
  DCHECK(sii);

  if (base::FeatureList::IsEnabled(kAlwaysUseMappableSIForOneCopyRaster)) {
    CHECK(!staging_buffer->mailbox.IsZero());
  } else {
    CHECK(staging_buffer->gpu_memory_buffer.get());
  }

  bool needs_clear = false;

  if (mailbox->IsZero()) {
    uint32_t usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_RASTER;
    if (mailbox_texture_is_overlay_candidate)
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    *mailbox = sii->CreateSharedImage(
        format, resource_size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, usage, "OneCopyRasterTile",
        gpu::kNullSurfaceHandle);
    // Clear the resource if we're not going to initialize it fully from the
    // copy due to non-exact resource reuse.  See https://crbug.com/1313091
    needs_clear = rect_to_copy.size() != resource_size;
  }

  // Create staging shared image.
  if (staging_buffer->mailbox.IsZero()) {
    const uint32_t usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE;
    auto client_shared_image = sii->CreateSharedImage(
        format, resource_size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, usage, "OneCopyRasterStaging",
        staging_buffer->gpu_memory_buffer.get()->CloneHandle());
    CHECK(client_shared_image);
    staging_buffer->mailbox = client_shared_image->mailbox();
  } else {
    sii->UpdateSharedImage(staging_buffer->sync_token, staging_buffer->mailbox);
  }

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  // Do not use queries unless COMMANDS_COMPLETED queries are supported, or
  // COMMANDS_ISSUED queries are sufficient.
  GLenum query_target = GL_NONE;

  if (worker_context_provider_->ContextCapabilities().sync_query) {
    // Use GL_COMMANDS_COMPLETED_CHROMIUM when supported because native
    // GpuMemoryBuffers can be accessed by the GPU after commands are issued
    // until GPU reads are done.
    query_target = GL_COMMANDS_COMPLETED_CHROMIUM;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_ARM_FAMILY)
  // TODO(reveman): This avoids a performance problem on ARM ChromeOS devices.
  // https://crbug.com/580166
  query_target = GL_COMMANDS_ISSUED_CHROMIUM;
#endif

  // COMMANDS_ISSUED is sufficient for shared memory resources.
  if (staging_buffer->is_shared_memory) {
    query_target = GL_COMMANDS_ISSUED_CHROMIUM;
  }

  if (query_target != GL_NONE) {
    if (!staging_buffer->query_id)
      ri->GenQueriesEXT(1, &staging_buffer->query_id);

    ri->BeginQueryEXT(query_target, staging_buffer->query_id);
  }

  // Clear to ensure the resource is fully initialized and BeginAccess succeeds.
  if (needs_clear) {
    int clear_bytes_per_row = viz::ResourceSizes::UncheckedWidthInBytes<int>(
        resource_size.width(), format);
    SkImageInfo dst_info = SkImageInfo::MakeN32Premul(resource_size.width(),
                                                      resource_size.height());
    SkBitmap bitmap;
    if (bitmap.tryAllocPixels(dst_info, clear_bytes_per_row)) {
      // SkBitmap.cpp doesn't yet have an interface for SkColor4fs
      // https://bugs.chromium.org/p/skia/issues/detail?id=13329
      bitmap.eraseColor(raster_source->background_color().toSkColor());
      ri->WritePixels(*mailbox, /*dst_x_offset=*/0, /*dst_y_offset=*/0,
                      /*dst_plane_index=*/0, mailbox_texture_target,
                      bitmap.pixmap());
    }
  }

  int bytes_per_row = viz::ResourceSizes::UncheckedWidthInBytes<int>(
      rect_to_copy.width(), staging_buffer->format);
  int chunk_size_in_rows =
      std::max(1, max_bytes_per_copy_operation_ / bytes_per_row);
  // Align chunk size to 4. Required to support compressed texture formats.
  chunk_size_in_rows = MathUtil::UncheckedRoundUp(chunk_size_in_rows, 4);
  int y = 0;
  int height = rect_to_copy.height();
  while (y < height) {
    // Copy at most |chunk_size_in_rows|.
    int rows_to_copy = std::min(chunk_size_in_rows, height - y);
    DCHECK_GT(rows_to_copy, 0);

    ri->CopySharedImage(
        staging_buffer->mailbox, *mailbox, mailbox_texture_target, 0, y, 0, y,
        rect_to_copy.width(), rows_to_copy, false /* unpack_flip_y */,
        false /* unpack_premultiply_alpha */);
    y += rows_to_copy;

    // Increment |bytes_scheduled_since_last_flush_| by the amount of memory
    // used for this copy operation.
    bytes_scheduled_since_last_flush_ += rows_to_copy * bytes_per_row;

    if (bytes_scheduled_since_last_flush_ >= max_bytes_per_copy_operation_) {
      ri->ShallowFlushCHROMIUM();
      bytes_scheduled_since_last_flush_ = 0;
    }
  }

  if (query_target != GL_NONE)
    ri->EndQueryEXT(query_target);

  // Generate sync token on the worker context that will be sent to and waited
  // for by the display compositor before using the content generated here.
  // The same sync token is used to synchronize operations on the staging
  // buffer. Note, the query completion is generally enough to guarantee
  // ordering, but there are some paths (e.g.
  // StagingBufferPool::ReduceMemoryUsage) that may destroy the staging buffer
  // without waiting for the query completion.
  gpu::SyncToken out_sync_token =
      viz::ClientResourceProvider::GenerateSyncTokenHelper(ri);
  staging_buffer->sync_token = out_sync_token;
  return out_sync_token;
}

}  // namespace cc
