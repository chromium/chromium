// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_EXTERNAL_STENCIL_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_EXTERNAL_STENCIL_H_

#include "android_webview/browser/gfx/aw_gl_surface.h"

// Lifetime: WebView
namespace android_webview {
class AwGLSurfaceExternalStencil : public AwGLSurface {
 public:
  AwGLSurfaceExternalStencil(gl::GLDisplayEGL* display, bool is_angle);

  AwGLSurfaceExternalStencil(const AwGLSurfaceExternalStencil&) = delete;
  AwGLSurfaceExternalStencil& operator=(const AwGLSurfaceExternalStencil&) =
      delete;

  unsigned int GetBackingFramebufferObject() override;
  gfx::SwapResult SwapBuffers(
      PresentationCallback callbackaw_gl_surface_external_stencil,
      gfx::FrameData data) override;
  void RecalculateClipAndTransform(gfx::Size* viewport,
                                   gfx::Rect* clip_rect,
                                   gfx::Transform* transform) override;
  bool IsDrawingToFBO() override;
  void DestroyExternalStencilFramebuffer() override;

 protected:
  ~AwGLSurfaceExternalStencil() override;

 private:
  class BlitContext;
  class FrameBuffer;

  // Contains GL shader and other info required to draw a quad.
  std::unique_ptr<BlitContext> blit_context_;
  std::unique_ptr<FrameBuffer> framebuffer_;
  gfx::Rect clip_rect_;
  gfx::Size viewport_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_SURFACE_EXTERNAL_STENCIL_H_
