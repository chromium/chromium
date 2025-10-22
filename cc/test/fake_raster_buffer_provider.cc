// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_buffer_provider.h"

#include <utility>

#include "cc/resources/resource_pool.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace cc {

FakeRasterBufferProviderImpl::FakeRasterBufferProviderImpl() = default;

FakeRasterBufferProviderImpl::~FakeRasterBufferProviderImpl() = default;

std::unique_ptr<RasterBuffer>
FakeRasterBufferProviderImpl::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  auto backing = std::make_unique<ResourcePool::Backing>(
      resource.size(), resource.format(), resource.color_space());
  backing->CreateSharedImageForTesting();
  resource.set_backing(std::move(backing));
  return nullptr;
}

void FakeRasterBufferProviderImpl::Flush() {}

bool FakeRasterBufferProviderImpl::CanPartialRasterIntoProvidedResource()
    const {
  return true;
}

bool FakeRasterBufferProviderImpl::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) {
  return true;
}

uint64_t FakeRasterBufferProviderImpl::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) {
  return 0;
}

void FakeRasterBufferProviderImpl::Shutdown() {}

}  // namespace cc
