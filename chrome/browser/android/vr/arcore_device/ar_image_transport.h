// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_IMAGE_TRANSPORT_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_IMAGE_TRANSPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/arcore_device/ar_renderer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/size_f.h"

namespace gl {
class SurfaceTexture;
}  // namespace gl

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
struct MailboxHolder;
struct SyncToken;
}  // namespace gpu

namespace vr {
class MailboxToSurfaceBridge;
class WebXrPresentationState;
struct WebXrSharedBuffer;
}  // namespace vr

namespace device {

using XrFrameCallback = base::RepeatingCallback<void(const gfx::Transform&)>;

// This class handles transporting WebGL rendered output from the GPU process's
// command buffer GL context to the local GL context, and compositing WebGL
// output onto the camera image using the local GL context.
class ArImageTransport {
 public:
  explicit ArImageTransport(
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge);
  virtual ~ArImageTransport();

  virtual void DestroySharedBuffers(vr::WebXrPresentationState* webxr);

  // All methods must be called on a valid GL thread. Initialization
  // must happen after the local GL context is ready for use. That
  // starts the asynchronous setup for the GPU process command buffer
  // GL context via MailboxToSurfaceBridge, and the callback is called
  // once that's complete.
  virtual void Initialize(vr::WebXrPresentationState* webxr,
                          base::OnceClosure callback);

  virtual GLuint GetCameraTextureId();

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  virtual gpu::MailboxHolder TransferFrame(vr::WebXrPresentationState* webxr,
                                           const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);
  virtual void CreateGpuFenceForSyncToken(
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>);
  virtual void CopyCameraImageToFramebuffer(const gfx::Size& frame_size,
                                            const gfx::Transform& uv_transform);
  virtual void CopyDrawnImageToFramebuffer(vr::WebXrPresentationState* webxr,
                                           const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);
  virtual void CopyTextureToFramebuffer(GLuint texture,
                                        const gfx::Size& frame_size,
                                        const gfx::Transform& uv_transform);
  virtual void WaitSyncToken(const gpu::SyncToken& sync_token);
  virtual void CopyMailboxToSurfaceAndSwap(const gfx::Size& frame_size,
                                           const gpu::MailboxHolder& mailbox);

  bool UseSharedBuffer() { return shared_buffer_draw_; }
  void SetFrameAvailableCallback(XrFrameCallback on_frame_available);

 private:
  std::unique_ptr<vr::WebXrSharedBuffer> CreateBuffer();
  void ResizeSharedBuffer(vr::WebXrPresentationState* webxr,
                          const gfx::Size& size,
                          vr::WebXrSharedBuffer* buffer);
  void ResizeSurface(const gfx::Size& size);
  bool IsOnGlThread() const;
  void OnMailboxBridgeReady(base::OnceClosure callback);
  void OnFrameAvailable();
  std::unique_ptr<ArRenderer> ar_renderer_;
  // samplerExternalOES texture for the camera image.
  GLuint camera_texture_id_arcore_ = 0;
  GLuint camera_fbo_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;

  // If true, use shared buffer transport aka DRAW_INTO_TEXTURE_MAILBOX.
  // If false, use Surface transport aka SUBMIT_AS_MAILBOX_HOLDER.
  bool shared_buffer_draw_ = false;

  // Used for Surface transport (Android N)
  //
  // samplerExternalOES texture data for WebXR content image.
  GLuint transport_texture_id_ = 0;
  gfx::Size surface_size_;
  scoped_refptr<gl::SurfaceTexture> transport_surface_texture_;
  gfx::Transform transport_surface_texture_uv_transform_;
  float transport_surface_texture_uv_matrix_[16];
  XrFrameCallback on_transport_frame_available_;

  // Must be last.
  base::WeakPtrFactory<ArImageTransport> weak_ptr_factory_{this};
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
