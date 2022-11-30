// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"

#include <string>

#include "android_webview/browser/gfx/scoped_app_gl_state_restore_impl.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore_impl_angle.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_surface_egl.h"

namespace android_webview {

namespace {

ScopedAppGLStateRestore* g_current_instance = nullptr;

}  // namespace

// static
ScopedAppGLStateRestore* ScopedAppGLStateRestore::Current() {
  DCHECK(g_current_instance);
  return g_current_instance;
}

ScopedAppGLStateRestore::ScopedAppGLStateRestore(CallMode mode,
                                                 bool save_restore) {
  DCHECK(!g_current_instance);
  g_current_instance = this;

  TRACE_EVENT0("android_webview", "AppGLStateSave");
  if (gl::GLSurfaceEGL::GetGLDisplayEGL()
          ->ext->b_EGL_ANGLE_external_context_and_surface) {
    impl_ = std::make_unique<internal::ScopedAppGLStateRestoreImplAngle>(
        mode, save_restore);
  } else {
    impl_ = std::make_unique<internal::ScopedAppGLStateRestoreImpl>(
        mode, save_restore);
  }
}

ScopedAppGLStateRestore::~ScopedAppGLStateRestore() {
  DCHECK_EQ(this, g_current_instance);
  g_current_instance = nullptr;

  TRACE_EVENT0("android_webview", "AppGLStateRestore");
  impl_ = nullptr;
}

StencilState ScopedAppGLStateRestore::stencil_state() const {
  return impl_->stencil_state();
}

int ScopedAppGLStateRestore::framebuffer_binding_ext() const {
  return impl_->framebuffer_binding_ext();
}

bool ScopedAppGLStateRestore::skip_draw() const {
  return impl_->skip_draw();
}

ScopedAppGLStateRestore::Impl::Impl() = default;
ScopedAppGLStateRestore::Impl::~Impl() = default;

}  // namespace android_webview
