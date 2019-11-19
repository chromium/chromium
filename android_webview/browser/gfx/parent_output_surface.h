// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_PARENT_OUTPUT_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_PARENT_OUTPUT_SURFACE_H_

#include "android_webview/browser/gfx/aw_gl_surface.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display/output_surface.h"

namespace gfx {
struct PresentationFeedback;
}

namespace android_webview {
class AwRenderThreadContextProvider;

class ParentOutputSurface : public viz::OutputSurface {
 public:
  ParentOutputSurface(
      scoped_refptr<AwGLSurface> gl_surface,
      scoped_refptr<AwRenderThreadContextProvider> context_provider);
  ~ParentOutputSurface() override;

  // OutputSurface overrides.
  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& rect) override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  unsigned UpdateGpuFence() override;
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

 private:
  void OnPresentation(const gfx::PresentationFeedback& feedback);

  viz::OutputSurfaceClient* client_ = nullptr;
  // This is really a layering violation but needed for hooking up presentation
  // feedbacks properly.
  scoped_refptr<AwGLSurface> gl_surface_;
  base::WeakPtrFactory<ParentOutputSurface> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ParentOutputSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_PARENT_OUTPUT_SURFACE_H_
