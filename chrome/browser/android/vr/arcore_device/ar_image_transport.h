// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_IMAGE_TRANSPORT_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_IMAGE_TRANSPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/arcore_device/ar_renderer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/size_f.h"

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace vr {
class MailboxToSurfaceBridge;
}  // namespace vr

namespace device {

struct SharedFrameBuffer;
struct SharedFrameBufferSwapChain;

// This class copies the camera texture to a shared image and returns a mailbox
// holder which is suitable for mojo transport to the Renderer.
class ArImageTransport {
 public:
  explicit ArImageTransport(
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge);
  virtual ~ArImageTransport();

  // Initialize() must be called on a valid GL thread.
  virtual bool Initialize();

  virtual GLuint GetCameraTextureId();

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  virtual gpu::MailboxHolder TransferFrame(const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);

 private:
  void SetupHardwareBuffers();
  void ResizeSharedBuffer(const gfx::Size& size, SharedFrameBuffer* buffer);
  bool IsOnGlThread() const;
  std::unique_ptr<ArRenderer> ar_renderer_;
  // samplerExternalOES texture data for WebXR content image.
  GLuint camera_texture_id_arcore_ = 0;
  GLuint camera_fbo_ = 0;
  GLuint transfer_fbo_ = 0;
  bool transfer_fbo_completeness_checked_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<SharedFrameBufferSwapChain> swap_chain_;

  DISALLOW_COPY_AND_ASSIGN(ArImageTransport);
};

class ArImageTransportFactory {
 public:
  virtual ~ArImageTransportFactory() = default;
  virtual std::unique_ptr<ArImageTransport> Create(
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_IMAGE_TRANSPORT_H_
