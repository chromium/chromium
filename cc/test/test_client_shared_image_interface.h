// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define CC_TEST_TEST_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace cc {

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  TestGpuChannelHost();

 protected:
  ~TestGpuChannelHost() override;
};

class TestClientSharedImageInterface : public gpu::ClientSharedImageInterface {
 public:
  TestClientSharedImageInterface(
      scoped_refptr<gpu::SharedImageInterface> shared_image_interface);
  gpu::SyncToken GenVerifiedSyncToken() override;
  gpu::SyncToken GenUnverifiedSyncToken() override;
  void VerifySyncToken(gpu::SyncToken& sync_token) override;
  bool CanVerifySyncToken(const gpu::SyncToken& sync_token) override;
  void VerifyFlush() override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImageForSoftwareCompositor(
      const gpu::SharedImageInfo& si_info) override;

 protected:
  ~TestClientSharedImageInterface() override;

  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;
};

}  // namespace cc
#endif  // CC_TEST_TEST_CLIENT_SHARED_IMAGE_INTERFACE_H_
