// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_
#define CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_

#include <memory>
#include <vector>

#include "cc/raster/raster_buffer_provider.h"

namespace cc {

// Fake RasterBufferProvider that just no-ops all calls.
class FakeRasterBufferProviderImpl : public RasterBufferProvider {
 public:
  FakeRasterBufferProviderImpl();
  ~FakeRasterBufferProviderImpl() override;

  // RasterBufferProvider methods.
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
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_
