// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class LayerTreeFrameSink;

class CC_EXPORT BitmapRasterBufferProvider : public RasterBufferProvider {
 public:
  BitmapRasterBufferProvider(const BitmapRasterBufferProvider&) = delete;
  ~BitmapRasterBufferProvider() override;

  BitmapRasterBufferProvider& operator=(const BitmapRasterBufferProvider&) =
      delete;

  explicit BitmapRasterBufferProvider(LayerTreeFrameSink* frame_sink);

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override;
  viz::SharedImageFormat GetFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) override;
  void Shutdown() override;

 protected:
  void Flush() override;

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  const raw_ptr<LayerTreeFrameSink> frame_sink_;
};

}  // namespace cc

#endif  // CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
