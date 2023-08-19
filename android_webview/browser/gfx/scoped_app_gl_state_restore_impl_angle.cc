// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/scoped_app_gl_state_restore_impl_angle.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "base/android/build_info.h"
#include "base/native_library.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gl/gl_context.h"

namespace android_webview {
namespace {
namespace os {

// TODO(penghuang): remove this typedef when egl headers are updated to 1.5.
typedef __eglMustCastToProperFunctionPointerType(
    EGLAPIENTRYP PFNEGLGETPROCADDRESSPROC)(const char* procname);
typedef EGLContext(EGLAPIENTRYP PFNEGLGETCURRENTCONTEXTPROC)(void);

PFNEGLGETPROCADDRESSPROC eglGetProcAddressFn = nullptr;
PFNGLGETBOOLEANVPROC glGetBooleanvFn = nullptr;
PFNGLGETINTEGERVPROC glGetIntegervFn = nullptr;
PFNGLGETERRORPROC glGetErrorFn = nullptr;

#if DCHECK_IS_ON()
PFNEGLGETCURRENTCONTEXTPROC eglGetCurrentContextFn = nullptr;
#endif

template <typename T>
void AssignProc(T& fn, const char* name) {
  fn = reinterpret_cast<T>(eglGetProcAddressFn(name));
  CHECK(fn) << "Failed to get " << name;
}

void InitializeGLBindings() {
  if (eglGetProcAddressFn)
    return;

  base::NativeLibraryLoadError error;
  base::FilePath filename("libEGL.so");
  base::NativeLibrary egl_library = base::LoadNativeLibrary(filename, &error);
  CHECK(egl_library) << "Failed to load " << filename.MaybeAsASCII() << ": "
                     << error.ToString();

  eglGetProcAddressFn = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(
      base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                "eglGetProcAddress"));
  CHECK(eglGetProcAddressFn) << "Failed to get eglGetProcAddress.";

  AssignProc(glGetBooleanvFn, "glGetBooleanv");
  AssignProc(glGetIntegervFn, "glGetIntegerv");
  AssignProc(glGetErrorFn, "glGetError");

#if DCHECK_IS_ON()
  AssignProc(eglGetCurrentContextFn, "eglGetCurrentContext");
#endif
}

bool ClearGLErrors(bool warn, const char* msg) {
  bool no_error = true;
  GLenum error;
#if DCHECK_IS_ON()
  DCHECK(eglGetCurrentContextFn());
#endif
  while ((error = glGetErrorFn()) != GL_NO_ERROR) {
    DLOG_IF(WARNING, warn) << error << " " << msg;
    no_error = false;
  }

  return no_error;
}

}  // namespace os

}  // namespace

namespace internal {

ScopedAppGLStateRestoreImplAngle::ScopedAppGLStateRestoreImplAngle(
    ScopedAppGLStateRestore::CallMode mode,
    bool save_restore) {
  os::InitializeGLBindings();

  os::ClearGLErrors(true, "Incoming GLError");

#if DCHECK_IS_ON()
  egl_context_ = os::eglGetCurrentContextFn();
  DCHECK_NE(egl_context_, EGL_NO_CONTEXT) << " no native context is current.";
#endif

  if (mode == ScopedAppGLStateRestore::MODE_DRAW &&
      base::android::BuildInfo::GetInstance()->sdk_int() ==
          base::android::SDK_VERSION_S) {
    GLint red_bits = 0;
    GLint green_bits = 0;
    GLint blue_bits = 0;
    GLint alpha_bits = 0;
    os::glGetIntegervFn(GL_RED_BITS, &red_bits);
    os::glGetIntegervFn(GL_GREEN_BITS, &green_bits);
    os::glGetIntegervFn(GL_BLUE_BITS, &blue_bits);
    os::glGetIntegervFn(GL_ALPHA_BITS, &alpha_bits);
    skip_draw_ =
        red_bits == 8 && green_bits == 0 && blue_bits == 0 && alpha_bits == 0;
  }

  // Query |stencil_state_| with native GL API.
  // Android should have made a native EGL context current, so we can call GL
  // directly.
  os::glGetBooleanvFn(GL_STENCIL_TEST, &stencil_state_.stencil_test_enabled);
  os::glGetIntegervFn(GL_STENCIL_FUNC, &stencil_state_.stencil_front_func);
  os::glGetIntegervFn(GL_STENCIL_VALUE_MASK,
                      &stencil_state_.stencil_front_mask);
  os::glGetIntegervFn(GL_STENCIL_REF, &stencil_state_.stencil_front_ref);
  os::glGetIntegervFn(GL_STENCIL_BACK_FUNC, &stencil_state_.stencil_back_func);
  os::glGetIntegervFn(GL_STENCIL_BACK_VALUE_MASK,
                      &stencil_state_.stencil_back_mask);
  os::glGetIntegervFn(GL_STENCIL_BACK_REF, &stencil_state_.stencil_back_ref);
  os::glGetIntegervFn(GL_STENCIL_CLEAR_VALUE, &stencil_state_.stencil_clear);
  os::glGetIntegervFn(GL_STENCIL_WRITEMASK,
                      &stencil_state_.stencil_front_writemask);
  os::glGetIntegervFn(GL_STENCIL_BACK_WRITEMASK,
                      &stencil_state_.stencil_back_writemask);
  os::glGetIntegervFn(GL_STENCIL_FAIL, &stencil_state_.stencil_front_fail_op);
  os::glGetIntegervFn(GL_STENCIL_PASS_DEPTH_FAIL,
                      &stencil_state_.stencil_front_z_fail_op);
  os::glGetIntegervFn(GL_STENCIL_PASS_DEPTH_PASS,
                      &stencil_state_.stencil_front_z_pass_op);
  os::glGetIntegervFn(GL_STENCIL_BACK_FAIL,
                      &stencil_state_.stencil_back_fail_op);
  os::glGetIntegervFn(GL_STENCIL_BACK_PASS_DEPTH_FAIL,
                      &stencil_state_.stencil_back_z_fail_op);
  os::glGetIntegervFn(GL_STENCIL_BACK_PASS_DEPTH_PASS,
                      &stencil_state_.stencil_back_z_pass_op);
  // ANGLE can wrap current native FBO to an EGLSurface which will be used
  // later, so with that EGLSurface as render target, the framebuffer binding is
  // always 0.
  framebuffer_binding_ext_ = 0;

  os::ClearGLErrors(false, nullptr);
}

ScopedAppGLStateRestoreImplAngle::~ScopedAppGLStateRestoreImplAngle() {
#if DCHECK_IS_ON()
  DCHECK_EQ(egl_context_, os::eglGetCurrentContextFn())
      << " the native context is changed.";
#endif

  // Do not leak GLError out of chromium.
  os::ClearGLErrors(true, "Chromium GLError");
}

}  // namespace internal
}  // namespace android_webview
