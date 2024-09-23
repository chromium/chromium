// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "ui/gfx/color_space.h"

namespace cc {
namespace {

class BitmapSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~BitmapSoftwareBacking() override {
    if (shared_image) {
      auto sii = frame_sink->shared_image_interface();
      if (sii) {
        sii->DestroySharedImage(mailbox_sync_token, std::move(shared_image));
      }
    } else {
      frame_sink->DidDeleteSharedBitmap(shared_bitmap_id);
    }
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
      pmd->CreateSharedMemoryOwnershipEdge(buffer_dump_guid, mapping.guid(),
                                           importance);
  }

  raw_ptr<LayerTreeFrameSink> frame_sink;
  base::WritableSharedMemoryMapping mapping;
};

class BitmapRasterBufferImpl : public RasterBuffer {
 public:
  BitmapRasterBufferImpl(const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         BitmapSoftwareBacking* backing,
                         uint64_t resource_content_id,
                         uint64_t previous_content_id)
      : resource_size_(size),
        color_space_(color_space),
        pixels_(backing->mapping.memory()),
        resource_has_previous_content_(
            resource_content_id && resource_content_id == previous_content_id),
        backing_(backing) {}
  BitmapRasterBufferImpl(const BitmapRasterBufferImpl&) = delete;
  BitmapRasterBufferImpl& operator=(const BitmapRasterBufferImpl&) = delete;

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override {
    TRACE_EVENT0("cc", "BitmapRasterBuffer::Playback");
    gfx::Rect playback_rect = raster_full_rect;
    if (resource_has_previous_content_) {
      playback_rect.Intersect(raster_dirty_rect);
    }
    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";

    size_t stride = 0u;
    viz::SharedImageFormat format = backing_->shared_image
                                        ? viz::SinglePlaneFormat::kBGRA_8888
                                        : viz::SinglePlaneFormat::kRGBA_8888;
    RasterBufferProvider::PlaybackToMemory(
        pixels_, format, resource_size_, stride, raster_source,
        raster_full_rect, playback_rect, transform, color_space_,
        /*gpu_compositing=*/false, playback_settings);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  const gfx::Size resource_size_;
  const gfx::ColorSpace color_space_;

  // `pixels_` is not a raw_ptr<...> for performance reasons: pointee is never
  // protected by BackupRefPtr, because the pointer comes either from using
  // `mmap`, MapViewOfFile or base::AllocPages directly.
  RAW_PTR_EXCLUSION void* const pixels_;
  bool resource_has_previous_content_;
  raw_ptr<BitmapSoftwareBacking> backing_;
};

}  // namespace

BitmapRasterBufferProvider::BitmapRasterBufferProvider(
    LayerTreeFrameSink* frame_sink)
    : frame_sink_(frame_sink) {}

BitmapRasterBufferProvider::~BitmapRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
BitmapRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  DCHECK_EQ(resource.format(), frame_sink_->shared_image_interface()
                                   ? viz::SinglePlaneFormat::kBGRA_8888
                                   : viz::SinglePlaneFormat::kRGBA_8888);

  const gfx::Size& size = resource.size();
  const gfx::ColorSpace& color_space = resource.color_space();
  if (!resource.software_backing()) {
    auto backing = std::make_unique<BitmapSoftwareBacking>();
    backing->frame_sink = frame_sink_;
    auto sii = frame_sink_->shared_image_interface();
    if (sii) {
      auto shared_image_mapping = sii->CreateSharedImage(
          {viz::SinglePlaneFormat::kBGRA_8888, size, color_space,
           gpu::SHARED_IMAGE_USAGE_CPU_WRITE, "BitmapRasterBufferProvider"});
      backing->shared_image = std::move(shared_image_mapping.shared_image);
      CHECK(backing->shared_image);
      backing->mapping = std::move(shared_image_mapping.mapping);
      backing->mailbox_sync_token = sii->GenVerifiedSyncToken();
    } else {
      backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
      base::MappedReadOnlyRegion shm =
          viz::bitmap_allocation::AllocateSharedBitmap(
              size, viz::SinglePlaneFormat::kRGBA_8888);
      backing->mapping = std::move(shm.mapping);
      frame_sink_->DidAllocateSharedBitmap(std::move(shm.region),
                                           backing->shared_bitmap_id);
    }

    resource.set_software_backing(std::move(backing));
  }
  BitmapSoftwareBacking* backing =
      static_cast<BitmapSoftwareBacking*>(resource.software_backing());

  return std::make_unique<BitmapRasterBufferImpl>(
      size, color_space, backing, resource_content_id, previous_content_id);
}

void BitmapRasterBufferProvider::Flush() {}

viz::SharedImageFormat BitmapRasterBufferProvider::GetFormat() const {
  return frame_sink_->shared_image_interface()
             ? viz::SinglePlaneFormat::kBGRA_8888
             : viz::SinglePlaneFormat::kRGBA_8888;
}

bool BitmapRasterBufferProvider::IsResourcePremultiplied() const {
  return true;
}

bool BitmapRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

bool BitmapRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) {
  // Bitmap resources are immediately ready to draw.
  return true;
}

uint64_t BitmapRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) {
  // Bitmap resources are immediately ready to draw.
  return 0;
}

void BitmapRasterBufferProvider::Shutdown() {}

}  // namespace cc
