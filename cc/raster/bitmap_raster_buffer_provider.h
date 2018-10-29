// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
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
  ~BitmapRasterBufferProvider() override;

  explicit BitmapRasterBufferProvider(LayerTreeFrameSink* frame_sink);

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat() const override;
  bool IsResourceSwizzleRequired() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      const base::Closure& callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;
  bool CheckRasterFinishedQueries() override;

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  LayerTreeFrameSink* const frame_sink_;

  DISALLOW_COPY_AND_ASSIGN(BitmapRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
