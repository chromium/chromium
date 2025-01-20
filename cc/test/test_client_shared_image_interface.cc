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
                         mojo::MessagePipeHandle(mojo::kInvalidHandleValue))) {
  // There is a "LeakSanitizer: detected memory leaks" on
  // mojo::SharedRemoteBase<mojo::AssociatedRemote<gpu::mojom::GpuChannel>> in
  // the multithread ASAN test when TestGpuChannelHost is created on the Main
  // thread and released on the Compositor thread. Because |remote_| is not
  // actually used in the tests, so it's reset here to avoid the memory leak at
  // the end.
  ResetChannelRemoteForTesting();
}

TestGpuChannelHost::~TestGpuChannelHost() = default;

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

scoped_refptr<gpu::ClientSharedImage>
TestClientSharedImageInterface::CreateSharedImageForSoftwareCompositor(
    const gpu::SharedImageInfo& si_info) {
  return shared_image_interface_->CreateSharedImageForSoftwareCompositor(
      si_info);
}

}  // namespace cc
