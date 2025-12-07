// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/zero_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/features.h"
#include "cc/resources/resource_pool.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "url/gurl.h"

namespace cc {
namespace {

constexpr static auto kBufferUsage = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

}

ZeroCopyRasterBufferImpl::ZeroCopyRasterBufferImpl(
    const ResourcePool::InUsePoolResource& in_use_resource,
    scoped_refptr<gpu::SharedImageInterface> sii,
    bool resource_has_previous_content,
    bool is_software)
    : sii_(sii),
      resource_has_previous_content_(resource_has_previous_content),
      is_software_(is_software) {
  if (!in_use_resource.backing()) {
    if (is_software) {
      in_use_resource.InstallSoftwareBacking(
          sii, "ZeroCopyRasterBufferProviderSoftware");
      in_use_resource.backing()->mailbox_sync_token =
          sii->GenUnverifiedSyncToken();
    } else {
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

ZeroCopyRasterBufferImpl::~ZeroCopyRasterBufferImpl() {
  // This raster task is complete, so if the backing's SharedImage was created
  // on a worker thread during the raster work that has now happened.
  backing_->can_access_shared_image_on_compositor_thread = true;

  // Drop the SharedImage if it was not possible to map it.
  if (failed_to_map_shared_image_) {
    backing_->shared_image()->UpdateDestructionSyncToken(gpu::SyncToken());
    backing_->clear_shared_image();
  }

  if (is_software_) {
    // The below work is specific to GPU compositing.
    return;
  }

  // If it was not possible to allocate the SharedImage
  // (https://crbug.com/554541) or it was not possible to map it, then we
  // don't have anything to give to the display compositor, so we report a
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

void ZeroCopyRasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    const GURL& url) {
  TRACE_EVENT0("cc", "ZeroCopyRasterBuffer::Playback");

  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content_) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  // Create a MappableSI if necessary.
  if (is_software_) {
    // WHen used with the software compositor, the SharedImage is created in the
    // constructor.
    CHECK(backing_->shared_image());
  } else if (!backing_->shared_image()) {
    gpu::SharedImageUsageSet usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    if (!backing_->CreateSharedImage(sii_.get(), usage, "ZeroCopyRasterTile",
                                     kBufferUsage)) {
      LOG(ERROR) << "Creation of MappableSharedImage failed.";
      return;
    }
  }

  std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping =
      backing_->shared_image()->Map();
  if (!mapping) {
    LOG(ERROR) << "MapSharedImage Failed.";

    // NOTE: It is not safe to clear the SharedImage here as it might be being
    // read on the compositor thread as part of memory dump generation.
    // Instead, save the fact that mapping failed so that the SharedImage can
    // be cleared in the destructor of this object.
    failed_to_map_shared_image_ = true;
    return;
  }

  // TODO(danakj): Implement partial raster with raster_dirty_rect for GPU
  // compositing.
  RasterBufferProvider::PlaybackToMemory(
      mapping->GetMemoryForPlane(0).data(), backing_->format(),
      backing_->size(), mapping->Stride(0), raster_source, raster_full_rect,
      playback_rect, transform, backing_->color_space(), playback_settings);
}

bool ZeroCopyRasterBufferImpl::SupportsBackgroundThreadPriority() const {
  return true;
}

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    const scoped_refptr<gpu::SharedImageInterface>& shared_image_interface,
    bool is_software)
    : is_software_(is_software),
      shared_image_interface_(shared_image_interface) {
  CHECK(shared_image_interface_)
      << "SharedImageInterface is null in ZeroCopyRasterBufferProvider ctor!";
}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;

  return std::make_unique<ZeroCopyRasterBufferImpl>(
      resource, shared_image_interface_, resource_has_previous_content,
      is_software_);
}

void ZeroCopyRasterBufferProvider::Flush() {}

bool ZeroCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource()
    const {
  return true;
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
