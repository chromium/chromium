// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {

class CC_EXPORT ZeroCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ZeroCopyRasterBufferProvider(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      viz::ContextProvider* compositor_context_provider,
      viz::ResourceFormat tile_format);
  ZeroCopyRasterBufferProvider(const ZeroCopyRasterBufferProvider&) = delete;
  ~ZeroCopyRasterBufferProvider() override;

  ZeroCopyRasterBufferProvider& operator=(const ZeroCopyRasterBufferProvider&) =
      delete;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;
  bool CheckRasterFinishedQueries() override;

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::ContextProvider* compositor_context_provider_;
  viz::ResourceFormat tile_format_;
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
