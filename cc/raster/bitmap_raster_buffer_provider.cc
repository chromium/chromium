// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/raster/raster_buffer.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "ui/gfx/color_space.h"
#include "url/gurl.h"

namespace cc {
namespace {

class BitmapRasterBufferImpl : public RasterBuffer {
 public:
  BitmapRasterBufferImpl(const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         ResourcePool::Backing* backing,
                         uint64_t resource_content_id,
                         uint64_t previous_content_id)
      : resource_size_(size),
        color_space_(color_space),
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
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kBGRA_8888;
    auto mapping = backing_->shared_image()->Map();
    void* memory = mapping->GetMemoryForPlane(0).data();
    RasterBufferProvider::PlaybackToMemory(
        memory, format, resource_size_, stride, raster_source, raster_full_rect,
        playback_rect, transform, color_space_, playback_settings);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  const gfx::Size resource_size_;
  const gfx::ColorSpace color_space_;

  bool resource_has_previous_content_;
  raw_ptr<ResourcePool::Backing> backing_;
};

}  // namespace

BitmapRasterBufferProvider::BitmapRasterBufferProvider(
    LayerTreeFrameSink* frame_sink)
    : shared_image_interface_(frame_sink->shared_image_interface()) {
  CHECK(shared_image_interface_)
      << "SharedImageInterface is null in BitmapRasterBufferProvider ctor!";
}

BitmapRasterBufferProvider::~BitmapRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
BitmapRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  DCHECK_EQ(resource.format(), viz::SinglePlaneFormat::kBGRA_8888);

  if (!resource.backing()) {
    resource.InstallSoftwareBacking(shared_image_interface_,
                                    "BitmapRasterBufferProvider");

    resource.backing()->mailbox_sync_token =
        shared_image_interface_->GenVerifiedSyncToken();
  }
  ResourcePool::Backing* backing = resource.backing();

  return std::make_unique<BitmapRasterBufferImpl>(
      resource.size(), resource.color_space(), backing, resource_content_id,
      previous_content_id);
}

void BitmapRasterBufferProvider::Flush() {}

viz::SharedImageFormat BitmapRasterBufferProvider::GetFormat() const {
  return viz::SinglePlaneFormat::kBGRA_8888;
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
