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
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "ui/gfx/color_space.h"
#include "url/gurl.h"

namespace cc {

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
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<ZeroCopyRasterBufferImpl>(
      resource, shared_image_interface_, resource_has_previous_content,
      /*is_software=*/true);
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
