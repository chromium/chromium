// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_ANGLE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_ANGLE_H_

#include <EGL/egl.h>

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "base/dcheck_is_on.h"

namespace android_webview {
namespace internal {

// Lifetime: Temporary
class ScopedAppGLStateRestoreImplAngle : public ScopedAppGLStateRestore::Impl {
 public:
  ScopedAppGLStateRestoreImplAngle(ScopedAppGLStateRestore::CallMode mode,
                                   bool save_restore);

  ScopedAppGLStateRestoreImplAngle(const ScopedAppGLStateRestoreImplAngle&) =
      delete;
  ScopedAppGLStateRestoreImplAngle& operator=(
      const ScopedAppGLStateRestoreImplAngle&) = delete;

  ~ScopedAppGLStateRestoreImplAngle() override;

 private:
#if DCHECK_IS_ON()
  EGLContext egl_context_ = EGL_NO_CONTEXT;
#endif
};

}  // namespace internal
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_ANGLE_H_
