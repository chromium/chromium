// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_buffer_provider.h"

#include "cc/resources/resource_pool.h"

namespace cc {

class StubGpuBacking : public ResourcePool::GpuBacking {
 public:
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}
};

FakeRasterBufferProviderImpl::FakeRasterBufferProviderImpl() = default;

FakeRasterBufferProviderImpl::~FakeRasterBufferProviderImpl() = default;

std::unique_ptr<RasterBuffer>
FakeRasterBufferProviderImpl::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  auto backing = std::make_unique<StubGpuBacking>();
  backing->mailbox = gpu::Mailbox::Generate();
  resource.set_gpu_backing(std::move(backing));
  return nullptr;
}

void FakeRasterBufferProviderImpl::Flush() {}

viz::ResourceFormat FakeRasterBufferProviderImpl::GetResourceFormat() const {
  return viz::ResourceFormat::RGBA_8888;
}

bool FakeRasterBufferProviderImpl::IsResourceSwizzleRequired() const {
  return false;
}

bool FakeRasterBufferProviderImpl::IsResourcePremultiplied() const {
  return true;
}

bool FakeRasterBufferProviderImpl::CanPartialRasterIntoProvidedResource()
    const {
  return true;
}

bool FakeRasterBufferProviderImpl::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  return true;
}

uint64_t FakeRasterBufferProviderImpl::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Callback<void()>& callback,
    uint64_t pending_callback_id) const {
  return 0;
}

void FakeRasterBufferProviderImpl::Shutdown() {}

bool FakeRasterBufferProviderImpl::CheckRasterFinishedQueries() {
  return false;
}

}  // namespace cc
