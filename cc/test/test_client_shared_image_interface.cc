// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_client_shared_image_interface.h"

#include <utility>

namespace cc {

TestGpuChannelHost::TestGpuChannelHost()
    : GpuChannelHost(0 /* channel_id */,
                     gpu::GPUInfo(),
                     gpu::GpuFeatureInfo(),
                     gpu::SharedImageCapabilities(),
                     mojo::ScopedMessagePipeHandle(
                         mojo::MessagePipeHandle(mojo::kInvalidHandleValue))) {}

TestGpuChannelHost::~TestGpuChannelHost() = default;

gpu::mojom::GpuChannel& TestGpuChannelHost::GetGpuChannel() {
  return gpu_channel_;
}

TestClientSharedImageInterface::TestClientSharedImageInterface(
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface)
    : gpu::ClientSharedImageInterface(
          nullptr,
          base::MakeRefCounted<TestGpuChannelHost>()),
      shared_image_interface_(std::move(shared_image_interface)) {}

TestClientSharedImageInterface::~TestClientSharedImageInterface() = default;

gpu::SyncToken TestClientSharedImageInterface::GenVerifiedSyncToken() {
  return shared_image_interface_->GenVerifiedSyncToken();
}

gpu::SharedImageInterface::SharedImageMapping
TestClientSharedImageInterface::CreateSharedImage(
    const gpu::SharedImageInfo& si_info) {
  return shared_image_interface_->CreateSharedImage(si_info);
}

}  // namespace cc
