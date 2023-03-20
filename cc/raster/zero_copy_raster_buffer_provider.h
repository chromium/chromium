// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
class WaitableEvent;
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
      viz::SharedImageFormat tile_format);
  ZeroCopyRasterBufferProvider(const ZeroCopyRasterBufferProvider&) = delete;
  ~ZeroCopyRasterBufferProvider() override;

  ZeroCopyRasterBufferProvider& operator=(const ZeroCopyRasterBufferProvider&) =
      delete;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override;
  void Flush() override;
  viz::SharedImageFormat GetFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) const override;
  void SetShutdownEvent(base::WaitableEvent* shutdown_event) override;
  void Shutdown() override;

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  raw_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  raw_ptr<base::WaitableEvent> shutdown_event_ = nullptr;
  raw_ptr<viz::ContextProvider> compositor_context_provider_;
  viz::SharedImageFormat tile_format_;
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
