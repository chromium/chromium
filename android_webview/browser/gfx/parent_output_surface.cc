// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/parent_output_surface.h"

#include <utility>

#include "android_webview/browser/gfx/aw_render_thread_context_provider.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace android_webview {

ParentOutputSurface::ParentOutputSurface(
    scoped_refptr<AwGLSurface> gl_surface,
    scoped_refptr<AwRenderThreadContextProvider> context_provider)
    : viz::OutputSurface(std::move(context_provider)),
      gl_surface_(std::move(gl_surface)) {}

ParentOutputSurface::~ParentOutputSurface() {
}

void ParentOutputSurface::BindToClient(viz::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void ParentOutputSurface::EnsureBackbuffer() {}

void ParentOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void ParentOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ParentOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {}

void ParentOutputSurface::Reshape(const gfx::Size& size,
                                  float scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha,
                                  bool use_stencil) {}

void ParentOutputSurface::SwapBuffers(viz::OutputSurfaceFrame frame) {
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  gl_surface_->SwapBuffers(base::BindOnce(&ParentOutputSurface::OnPresentation,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ParentOutputSurface::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
}

bool ParentOutputSurface::HasExternalStencilTest() const {
  return ScopedAppGLStateRestore::Current()
      ->stencil_state()
      .stencil_test_enabled;
}

void ParentOutputSurface::ApplyExternalStencil() {
  StencilState stencil_state =
      ScopedAppGLStateRestore::Current()->stencil_state();
  DCHECK(stencil_state.stencil_test_enabled);
  gpu::gles2::GLES2Interface* gl = context_provider()->ContextGL();
  gl->StencilFuncSeparate(GL_FRONT, stencil_state.stencil_front_func,
                          stencil_state.stencil_front_mask,
                          stencil_state.stencil_front_ref);
  gl->StencilFuncSeparate(GL_BACK, stencil_state.stencil_back_func,
                          stencil_state.stencil_back_mask,
                          stencil_state.stencil_back_ref);
  gl->StencilMaskSeparate(GL_FRONT, stencil_state.stencil_front_writemask);
  gl->StencilMaskSeparate(GL_BACK, stencil_state.stencil_back_writemask);
  gl->StencilOpSeparate(GL_FRONT, stencil_state.stencil_front_fail_op,
                        stencil_state.stencil_front_z_fail_op,
                        stencil_state.stencil_front_z_pass_op);
  gl->StencilOpSeparate(GL_BACK, stencil_state.stencil_back_fail_op,
                        stencil_state.stencil_back_z_fail_op,
                        stencil_state.stencil_back_z_pass_op);
}

uint32_t ParentOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl = static_cast<AwRenderThreadContextProvider*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

bool ParentOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned ParentOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat ParentOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

unsigned ParentOutputSurface::UpdateGpuFence() {
  return 0;
}

void ParentOutputSurface::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {}

gfx::OverlayTransform ParentOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace android_webview
