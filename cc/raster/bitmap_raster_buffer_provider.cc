// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/process/memory.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

base::UnsafeSharedMemoryRegion AllocateSharedMemory(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  DCHECK(format.IsBitmapFormatSupported())
      << "(format = " << format.ToString() << ")";

  size_t bytes = 0;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &bytes)) {
    DLOG(ERROR) << "AllocateMappedBitmap with size that overflows";
    size_t alloc_size = std::numeric_limits<int>::max();
    base::TerminateBecauseOutOfMemory(alloc_size);
  }

  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(bytes);
  if (!shared_memory.IsValid()) {
    DLOG(ERROR) << "Browser failed to allocate shared memory";
    base::TerminateBecauseOutOfMemory(bytes);
  }
  return shared_memory;
}

class BitmapSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~BitmapSoftwareBacking() override {
    if (frame_sink->shared_image_interface()) {
      frame_sink->shared_image_interface()->DestroySharedImage(
          gpu::SyncToken(), shared_bitmap_id);
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

  base::UnsafeSharedMemoryRegion unsafe_region;
};

class BitmapRasterBufferImpl : public RasterBuffer {
 public:
  BitmapRasterBufferImpl(const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         void* pixels,
                         uint64_t resource_content_id,
                         uint64_t previous_content_id)
      : resource_size_(size),
        color_space_(color_space),
        pixels_(pixels),
        resource_has_previous_content_(
            resource_content_id && resource_content_id == previous_content_id) {
  }
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
    RasterBufferProvider::PlaybackToMemory(
        pixels_, viz::SinglePlaneFormat::kRGBA_8888, resource_size_, stride,
        raster_source, raster_full_rect, playback_rect, transform, color_space_,
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
  DCHECK_EQ(resource.format(), viz::SinglePlaneFormat::kRGBA_8888);

  const gfx::Size& size = resource.size();
  const gfx::ColorSpace& color_space = resource.color_space();
  if (!resource.software_backing()) {
    auto backing = std::make_unique<BitmapSoftwareBacking>();
    backing->frame_sink = frame_sink_;

    if (frame_sink_->shared_image_interface()) {
      constexpr char kDebugLabel[] = "BitmapRasterBufferProvider";
      backing->unsafe_region =
          AllocateSharedMemory(size, viz::SinglePlaneFormat::kRGBA_8888);
      backing->mapping = backing->unsafe_region.Map();

      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.offset = 0;
      handle.stride = static_cast<int32_t>(gfx::RowSizeForBufferFormat(
          size.width(), gfx::BufferFormat::RGBA_8888, 0));
      handle.region = backing->unsafe_region.Duplicate();

      backing->shared_bitmap_id =
          frame_sink_->shared_image_interface()->CreateSharedImage(
              viz::SinglePlaneFormat::kRGBA_8888, size, color_space,
              kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
              gpu::SHARED_IMAGE_USAGE_CPU_WRITE, kDebugLabel,
              std::move(handle));

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
      size, color_space, backing->mapping.memory(), resource_content_id,
      previous_content_id);
}

void BitmapRasterBufferProvider::Flush() {}

viz::SharedImageFormat BitmapRasterBufferProvider::GetFormat() const {
  return viz::SinglePlaneFormat::kRGBA_8888;
}

bool BitmapRasterBufferProvider::IsResourcePremultiplied() const {
  return true;
}

bool BitmapRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

bool BitmapRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  // Bitmap resources are immediately ready to draw.
  return true;
}

uint64_t BitmapRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) const {
  // Bitmap resources are immediately ready to draw.
  return 0;
}

void BitmapRasterBufferProvider::Shutdown() {}

}  // namespace cc
