// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_SESSION_H_

#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "components/arc/mojom/screen_capture.mojom.h"
#include "components/viz/common/gl_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/compositor/compositor_animation_observer.h"

class ScreenCaptureNotificationUI;

namespace aura {
class Window;
}  // namespace aura

namespace content {
struct DesktopMediaID;
}  // namespace content

namespace gfx {
class ClientNativePixmapFactory;
class Size;
}  // namespace gfx

namespace viz {
class CopyOutputResult;
}  // namespace viz

namespace arc {

class ArcScreenCaptureSession : public mojom::ScreenCaptureSession,
                                public ui::CompositorAnimationObserver {
 public:
  // Creates a new ScreenCaptureSession and returns the interface pointer for
  // passing back across a Mojo pipe. This object will be automatically
  // destructed when the Mojo connection is closed.
  static mojom::ScreenCaptureSessionPtr Create(
      mojom::ScreenCaptureSessionNotifierPtr notifier,
      const std::string& display_name,
      content::DesktopMediaID desktop_id,
      const gfx::Size& size,
      bool enable_notification);

  // Implements mojo::ScreenCaptureSession interface.
  void SetOutputBuffer(mojo::ScopedHandle graphics_buffer,
                       uint32_t stride,
                       SetOutputBufferCallback callback) override;

  // Implements ui::CompositorAnimationObserver.
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

 private:
  struct DesktopTexture;
  struct PendingBuffer;

  ArcScreenCaptureSession(mojom::ScreenCaptureSessionNotifierPtr notifier,
                          const gfx::Size& size);
  ~ArcScreenCaptureSession() override;

  // Does additional checks and upon success returns a valid InterfacePtr, null
  // otherwise.
  mojom::ScreenCaptureSessionPtr Initialize(content::DesktopMediaID desktop_id,
                                            const std::string& display_name,
                                            bool enable_notification);
  // Copies the GL texture from a desktop capture to the corresponding GL
  // texture for a GPU buffer.
  void CopyDesktopTextureToGpuBuffer(
      std::unique_ptr<DesktopTexture> desktop_texture,
      std::unique_ptr<PendingBuffer> pending_buffer);
  // Closes the Mojo connection by destroying this object.
  void Close();
  // Callback for when we perform CopyOutputRequests.
  void OnDesktopCaptured(std::unique_ptr<viz::CopyOutputResult> result);
  // Callback for completion of GL commands.
  void QueryCompleted(GLuint query_id,
                      std::unique_ptr<PendingBuffer> pending_buffer);
  // Callback for a user clicking Stop on the notification for screen capture.
  void NotificationStop();

  mojo::Binding<mojom::ScreenCaptureSession> binding_;
  mojom::ScreenCaptureSessionNotifierPtr notifier_;
  gfx::Size size_;
  // aura::Window of the display being captured. This corresponds to one of
  // Ash's root windows.
  aura::Window* display_root_window_ = nullptr;

  // We have 2 separate queues for handling incoming GPU buffers from Android
  // and also textures for the desktop we have captured already. Due to the
  // parallel nature of the operations, we can end up with starvation in a queue
  // if we only implemented a queue for one end of this. This technique allows
  // us to maximize throughput and never have overhead with frame duplication as
  // well as never skip any output buffers.
  std::queue<std::unique_ptr<PendingBuffer>> buffer_queue_;
  std::queue<std::unique_ptr<DesktopTexture>> texture_queue_;
  std::unique_ptr<viz::GLHelper> gl_helper_;
  std::unique_ptr<viz::GLHelper::ScalerInterface> scaler_;
  std::unique_ptr<ScreenCaptureNotificationUI> notification_ui_;
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;

  base::WeakPtrFactory<ArcScreenCaptureSession> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcScreenCaptureSession);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_SESSION_H_
