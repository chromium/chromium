// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace gpu {
class SharedImageInterface;
}

namespace cc {

// RasterBuffer for the zero copy upload, which is given to the raster worker
// threads for raster/upload.
class ZeroCopyRasterBufferImpl : public RasterBuffer {
 public:
  ZeroCopyRasterBufferImpl(
      const ResourcePool::InUsePoolResource& in_use_resource,
      scoped_refptr<gpu::SharedImageInterface> sii,
      bool resource_has_previous_content,
      bool is_software);

  ZeroCopyRasterBufferImpl(const ZeroCopyRasterBufferImpl&) = delete;

  ~ZeroCopyRasterBufferImpl() override;

  ZeroCopyRasterBufferImpl& operator=(const ZeroCopyRasterBufferImpl&) = delete;

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override;

  bool SupportsBackgroundThreadPriority() const override;

 private:
  // These fields are safe to access on both the compositor and worker thread.
  raw_ptr<ResourcePool::Backing> backing_;
  const scoped_refptr<gpu::SharedImageInterface> sii_;
  bool failed_to_map_shared_image_ = false;
  bool resource_has_previous_content_ = false;
  bool is_software_ = false;
};

class CC_EXPORT ZeroCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ZeroCopyRasterBufferProvider(
      const scoped_refptr<gpu::SharedImageInterface>& shared_image_interface,
      bool is_software);

  ZeroCopyRasterBufferProvider(const ZeroCopyRasterBufferProvider&) = delete;
  ~ZeroCopyRasterBufferProvider() override;

  ZeroCopyRasterBufferProvider& operator=(const ZeroCopyRasterBufferProvider&) =
      delete;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
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

  bool is_software_ = false;
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
