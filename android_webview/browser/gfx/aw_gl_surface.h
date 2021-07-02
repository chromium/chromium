// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_surface_egl.h"

namespace android_webview {

// This surface is used to represent the underlying surface provided by the App
// inside a hardware draw. Note that offscreen contexts will not be using this
// GLSurface.
class AwGLSurface : public gl::GLSurfaceEGL {
 public:
  explicit AwGLSurface(bool is_angle);

  // Implement GLSurface.
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  unsigned int GetBackingFramebufferObject() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  void* GetDisplay() override;
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

 protected:
  ~AwGLSurface() override;

 private:
  const bool is_angle_;
  PresentationCallback pending_presentation_callback_;
  gfx::Size size_{1, 1};
  EGLSurface surface_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(AwGLSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_H_
