// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"

namespace android_webview {

// This surface is used to represent the underlying surface provided by the App
// inside a hardware draw. Note that offscreen contexts will not be using this
// GLSurface.
//
// Lifetime: WebView
class AwGLSurface : public gl::GLSurfaceEGL {
 public:
  AwGLSurface(gl::GLDisplayEGL* display, bool is_angle);
  AwGLSurface(gl::GLDisplayEGL* display, scoped_refptr<gl::GLSurface> surface);

  AwGLSurface(const AwGLSurface&) = delete;
  AwGLSurface& operator=(const AwGLSurface&) = delete;

  // Implement GLSurface.
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  unsigned int GetBackingFramebufferObject() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  gl::GLDisplay* GetGLDisplay() override;
  gl::GLSurfaceFormat GetFormat() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLConfig GetConfig() override;

  void SetSize(const gfx::Size& size);
  void MaybeDidPresent(const gfx::PresentationFeedback& feedback);

  virtual void RecalculateClipAndTransform(gfx::Size* viewport,
                                           gfx::Rect* clip_rect,
                                           gfx::Transform* transform) {}
  // Returns true if this GLSurface created fbo to implement stencil clipping.
  // This doesn't take into account if fbo was created by Android.
  virtual bool IsDrawingToFBO();
  virtual void DestroyExternalStencilFramebuffer() {}

  bool is_angle() { return is_angle_; }

  scoped_refptr<gl::GLSurface> wrapped_surface() const {
    return wrapped_surface_;
  }

 protected:
  ~AwGLSurface() override;

 private:
  const bool is_angle_;

  // This is used when when webview is compositing with vulkan. There are still
  // random code that expect to a real EGL context to be present to eg run
  // glGetError. A real EGL context requires a real EGL surface.
  // Note this is currently mutually exclusive with `is_angle_`.
  const scoped_refptr<gl::GLSurface> wrapped_surface_;

  PresentationCallback pending_presentation_callback_;
  gfx::Size size_{1, 1};
  EGLSurface surface_ = nullptr;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_
