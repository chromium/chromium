// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_gl_surface.h"

#include <utility>

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"

namespace android_webview {

AwGLSurface::AwGLSurface() : size_(1, 1) {}

AwGLSurface::~AwGLSurface() {}

void AwGLSurface::Destroy() {
}

bool AwGLSurface::IsOffscreen() {
  return false;
}

unsigned int AwGLSurface::GetBackingFramebufferObject() {
  return ScopedAppGLStateRestore::Current()->framebuffer_binding_ext();
}

gfx::SwapResult AwGLSurface::SwapBuffers(PresentationCallback callback) {
  DCHECK(pending_presentation_callback_.is_null());
  pending_presentation_callback_ = std::move(callback);
  return gfx::SwapResult::SWAP_ACK;
}

gfx::Size AwGLSurface::GetSize() {
  return size_;
}

void* AwGLSurface::GetHandle() {
  return NULL;
}

void* AwGLSurface::GetDisplay() {
  return NULL;
}

gl::GLSurfaceFormat AwGLSurface::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool AwGLSurface::Resize(const gfx::Size& size,
                         float scale_factor,
                         ColorSpace color_space,
                         bool has_alpha) {
  size_ = size;
  return true;
}

void AwGLSurface::SetSize(const gfx::Size& size) {
  size_ = size;
}

void AwGLSurface::MaybeDidPresent(gfx::PresentationFeedback feedback) {
  if (pending_presentation_callback_.is_null())
    return;
  std::move(pending_presentation_callback_).Run(std::move(feedback));
}

}  // namespace android_webview
