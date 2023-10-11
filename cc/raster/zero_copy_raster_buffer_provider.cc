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
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

BASE_FEATURE(kAlwaysUseMappableSIForZeroCopyRaster,
             "AlwaysUseMappableSIForZeroCopyRaster",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr static auto kBufferUsage = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

// Subclass for InUsePoolResource that holds ownership of a zero-copy backing
// and does cleanup of the backing when destroyed.
class ZeroCopyGpuBacking : public ResourcePool::GpuBacking {
 public:
  ~ZeroCopyGpuBacking() override {
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
    if (base::FeatureList::IsEnabled(kAlwaysUseMappableSIForZeroCopyRaster)) {
      if (mailbox.IsZero()) {
        return;
      }
      auto mapping = shared_image_interface->MapSharedImage(mailbox);
      if (!mapping) {
        return;
      }
      mapping->OnMemoryDump(pmd, buffer_dump_guid, tracing_process_id,
                            importance);
    } else {
      if (!gpu_memory_buffer) {
        return;
      }
      gpu_memory_buffer->OnMemoryDump(pmd, buffer_dump_guid, tracing_process_id,
                                      importance);
    }
  }

  // The SharedImageInterface used to clean up the shared image.
  raw_ptr<gpu::SharedImageInterface> shared_image_interface = nullptr;
  // The backing for zero-copy gpu resources. The |texture_id| is bound to
  // this.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
};

// RasterBuffer for the zero copy upload, which is given to the raster worker
// threads for raster/upload.
class ZeroCopyRasterBufferImpl : public RasterBuffer {
 public:
  ZeroCopyRasterBufferImpl(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      base::WaitableEvent* shutdown_event,
      const ResourcePool::InUsePoolResource& in_use_resource,
      ZeroCopyGpuBacking* backing)
      : backing_(backing),
        gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
        shutdown_event_(shutdown_event),
        resource_size_(in_use_resource.size()),
        format_(in_use_resource.format()),
        resource_color_space_(in_use_resource.color_space()),
        gpu_memory_buffer_(std::move(backing_->gpu_memory_buffer)) {}
  ZeroCopyRasterBufferImpl(const ZeroCopyRasterBufferImpl&) = delete;

  ~ZeroCopyRasterBufferImpl() override {
    // If MapSharedImage() or GpuMemoryBuffer allocation failed
    // (https://crbug.com/554541), then we don't have anything to give to the
    // display compositor, so we report a zero mailbox that will result in
    // checkerboarding.
    if (base::FeatureList::IsEnabled(kAlwaysUseMappableSIForZeroCopyRaster)) {
      CHECK(!gpu_memory_buffer_);
      if (backing_->mailbox.IsZero()) {
        return;
      }
    } else {
      if (!gpu_memory_buffer_) {
        DCHECK(backing_->mailbox.IsZero());
        return;
      }
    }

    // This is destroyed on the compositor thread when raster is complete, but
    // before the backing is prepared for export to the display compositor. So
    // we can set up the texture and SyncToken here.
    // TODO(danakj): This could be done with the worker context in Playback. Do
    // we need to do things in IsResourceReadyToDraw() and OrderingBarrier then?
    gpu::SharedImageInterface* sii = backing_->shared_image_interface;
    if (backing_->mailbox.IsZero()) {
      CHECK(
          !base::FeatureList::IsEnabled(kAlwaysUseMappableSIForZeroCopyRaster));
      uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                       gpu::SHARED_IMAGE_USAGE_SCANOUT;
      // Make a mailbox for export of the GpuMemoryBuffer to the display
      // compositor.
      backing_->mailbox = sii->CreateSharedImage(
          format_, resource_size_, resource_color_space_,
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
          "ZeroCopyRasterTile", gpu_memory_buffer_->CloneHandle());
    } else {
      sii->UpdateSharedImage(backing_->returned_sync_token, backing_->mailbox);
    }

    backing_->mailbox_sync_token = sii->GenUnverifiedSyncToken();
    backing_->gpu_memory_buffer = std::move(gpu_memory_buffer_);
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

    std::unique_ptr<gpu::SharedImageInterface::ScopedMapping> mapping;
    void* memory = nullptr;
    size_t stride = 0;

    if (base::FeatureList::IsEnabled(kAlwaysUseMappableSIForZeroCopyRaster)) {
      CHECK(!gpu_memory_buffer_);

      gpu::SharedImageInterface* sii = backing_->shared_image_interface;

      // Create a MappableSI if necessary.
      if (backing_->mailbox.IsZero()) {
        uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                         gpu::SHARED_IMAGE_USAGE_SCANOUT;
        backing_->mailbox = sii->CreateSharedImage(
            format_, resource_size_, resource_color_space_,
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
            "ZeroCopyRasterTile", gpu::kNullSurfaceHandle, kBufferUsage);
      }

      if (backing_->mailbox.IsZero()) {
        LOG(ERROR) << "Creation of MappableSharedImage failed.";
        return;
      }

      mapping = sii->MapSharedImage(backing_->mailbox);
      if (!mapping) {
        LOG(ERROR) << "MapSharedImage Failed.";
        sii->DestroySharedImage(gpu::SyncToken(), backing_->mailbox);
        backing_->mailbox.SetZero();
        return;
      }
      memory = mapping->Memory(0);
      stride = mapping->Stride(0);
    } else {
      if (!gpu_memory_buffer_) {
        gpu_memory_buffer_ = gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
            resource_size_,
            viz::SinglePlaneSharedImageFormatToBufferFormat(format_),
            kBufferUsage, gpu::kNullSurfaceHandle, shutdown_event_);
        // Note that GpuMemoryBuffer allocation can fail.
        // https://crbug.com/554541
        if (!gpu_memory_buffer_) {
          return;
        }
      }

      CHECK_EQ(1u, gfx::NumberOfPlanesForLinearBufferFormat(
                       gpu_memory_buffer_->GetFormat()));
      bool rv = gpu_memory_buffer_->Map();
      CHECK(rv);
      CHECK(gpu_memory_buffer_->memory(0));
      // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
      CHECK_GE(gpu_memory_buffer_->stride(0), 0);

      memory = gpu_memory_buffer_->memory(0);
      stride = gpu_memory_buffer_->stride(0);
    }

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    RasterBufferProvider::PlaybackToMemory(
        memory, format_, resource_size_, stride, raster_source,
        raster_full_rect, raster_full_rect, transform, resource_color_space_,
        /*gpu_compositing=*/true, playback_settings);

    base::FeatureList::IsEnabled(kAlwaysUseMappableSIForZeroCopyRaster)
        ? mapping.reset()
        : gpu_memory_buffer_->Unmap();
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  // This field may only be used on the compositor thread.
  raw_ptr<ZeroCopyGpuBacking> backing_;

  // These fields are for use on the worker thread.
  raw_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  raw_ptr<base::WaitableEvent> shutdown_event_;
  gfx::Size resource_size_;
  viz::SharedImageFormat format_;
  gfx::ColorSpace resource_color_space_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
};

}  // namespace

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    viz::RasterContextProvider* compositor_context_provider,
    const RasterCapabilities& raster_caps)
    : gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      compositor_context_provider_(compositor_context_provider),
      tile_format_(raster_caps.tile_format),
      tile_texture_target_(raster_caps.tile_texture_target) {}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<ZeroCopyGpuBacking>();
    backing->overlay_candidate = true;
    backing->texture_target = tile_texture_target_;
    // This RasterBufferProvider will modify the resource outside of the
    // GL command stream. So resources should not become available for reuse
    // until they are not in use by the gpu anymore, which a fence is used
    // to determine.
    backing->wait_on_fence_required = true;
    backing->shared_image_interface =
        compositor_context_provider_->SharedImageInterface();
    resource.set_gpu_backing(std::move(backing));
  }
  ZeroCopyGpuBacking* backing =
      static_cast<ZeroCopyGpuBacking*>(resource.gpu_backing());

  return std::make_unique<ZeroCopyRasterBufferImpl>(
      gpu_memory_buffer_manager_, shutdown_event_, resource, backing);
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

void ZeroCopyRasterBufferProvider::SetShutdownEvent(
    base::WaitableEvent* shutdown_event) {
  shutdown_event_ = shutdown_event;
}

void ZeroCopyRasterBufferProvider::Shutdown() {}

}  // namespace cc
