// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/zero_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/raster/raster_buffer.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/buffer_format_util.h"
#include "url/gurl.h"

namespace cc {
namespace {

constexpr static auto kBufferUsage = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

// RasterBuffer for the zero copy upload, which is given to the raster worker
// threads for raster/upload.
class ZeroCopyRasterBufferImpl : public RasterBuffer {
 public:
  ZeroCopyRasterBufferImpl(
      const ResourcePool::InUsePoolResource& in_use_resource,
      scoped_refptr<gpu::SharedImageInterface> sii)
      : sii_(sii) {
    if (!in_use_resource.backing()) {
      auto backing = std::make_unique<ResourcePool::Backing>(
          in_use_resource.size(), in_use_resource.format(),
          in_use_resource.color_space());
      // This RasterBufferProvider will modify the resource outside of the
      // GL command stream. So resources should not become available for reuse
      // until they are not in use by the gpu anymore, which a fence is used
      // to determine.
      backing->wait_on_fence_required = true;
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

  ZeroCopyRasterBufferImpl(const ZeroCopyRasterBufferImpl&) = delete;

  ~ZeroCopyRasterBufferImpl() override {
    // This raster task is complete, so if the backing's SharedImage was created
    // on a worker thread during the raster work that has now happened.
    backing_->can_access_shared_image_on_compositor_thread = true;

    // If MappableSharedImage allocation failed (https://crbug.com/554541), then
    // we don't have anything to give to the display compositor, so we report a
    // zero mailbox that will result in checkerboarding.
    if (!backing_->shared_image()) {
      return;
    }

    // This is destroyed on the compositor thread when raster is complete, but
    // before the backing is prepared for export to the display compositor. So
    // we can set up the texture and SyncToken here.
    // TODO(danakj): This could be done with the worker context in Playback. Do
    // we need to do things in IsResourceReadyToDraw() and OrderingBarrier then?
    sii_->UpdateSharedImage(backing_->returned_sync_token,
                            backing_->shared_image()->mailbox());

    backing_->mailbox_sync_token = sii_->GenUnverifiedSyncToken();
  }

  ZeroCopyRasterBufferImpl& operator=(const ZeroCopyRasterBufferImpl&) = delete;

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override {
    TRACE_EVENT0("cc", "ZeroCopyRasterBuffer::Playback");

    // Create a MappableSI if necessary.
    if (!backing_->shared_image()) {
      gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                       gpu::SHARED_IMAGE_USAGE_SCANOUT;
      backing_->set_shared_image(sii_->CreateSharedImage(
          {backing_->format(), backing_->size(), backing_->color_space(), usage,
           "ZeroCopyRasterTile"},
          gpu::kNullSurfaceHandle, kBufferUsage));
      if (!backing_->shared_image()) {
        LOG(ERROR) << "Creation of MappableSharedImage failed.";
        return;
      }
    }

    std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping =
        backing_->shared_image()->Map();
    if (!mapping) {
      LOG(ERROR) << "MapSharedImage Failed.";
      backing_->shared_image()->UpdateDestructionSyncToken(gpu::SyncToken());
      backing_->clear_shared_image();
      return;
    }

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    RasterBufferProvider::PlaybackToMemory(
        mapping->GetMemoryForPlane(0).data(), backing_->format(),
        backing_->size(), mapping->Stride(0), raster_source, raster_full_rect,
        raster_full_rect, transform, backing_->color_space(),
        playback_settings);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  // These fields are safe to access on both the compositor and worker thread.
  raw_ptr<ResourcePool::Backing> backing_;
  const scoped_refptr<gpu::SharedImageInterface> sii_;
};

}  // namespace

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    viz::RasterContextProvider* compositor_context_provider,
    const RasterCapabilities& raster_caps)
    : compositor_context_provider_(compositor_context_provider),
      tile_format_(raster_caps.tile_format) {}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  return std::make_unique<ZeroCopyRasterBufferImpl>(
      resource, base::WrapRefCounted(
                    compositor_context_provider_->SharedImageInterface()));
}

void ZeroCopyRasterBufferProvider::Flush() {}

viz::SharedImageFormat ZeroCopyRasterBufferProvider::GetFormat() const {
  return tile_format_;
}

bool ZeroCopyRasterBufferProvider::IsResourcePremultiplied() const {
  return true;
}

bool ZeroCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource()
    const {
  return false;
}

bool ZeroCopyRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) {
  // Zero-copy resources are immediately ready to draw.
  return true;
}

uint64_t ZeroCopyRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) {
  // Zero-copy resources are immediately ready to draw.
  return 0;
}

void ZeroCopyRasterBufferProvider::Shutdown() {}

}  // namespace cc
