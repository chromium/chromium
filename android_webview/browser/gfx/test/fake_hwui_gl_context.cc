// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/test/fake_hwui_gl_context.h"

#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/threading/thread_restrictions.h"

namespace android_webview {

FakeHWUIGLContext::FakeHWUIGLContext() {
  InitializeGLBindings();
}

FakeHWUIGLContext::~FakeHWUIGLContext() {
  if (gl_context_) {
    DestroyContext();
  }
}

void FakeHWUIGLContext::CreateContext(ANativeWindow* a_native_window) {
  {
    std::vector<EGLint> egl_window_attributes;
    egl_window_attributes.push_back(EGL_NONE);
    gl_surface_ = eglCreateWindowSurfaceFn(
        GetDisplay(), GetConfig(), a_native_window, &egl_window_attributes[0]);
    CHECK(gl_surface_);
  }

  {
    std::vector<EGLint> context_attributes = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                              EGL_NONE};

    CHECK(eglBindAPIFn(EGL_OPENGL_ES_API));

    gl_context_ = eglCreateContextFn(GetDisplay(), GetConfig(), nullptr,
                                     context_attributes.data());
    CHECK(gl_context_);
  }
}

void FakeHWUIGLContext::DestroyContext() {
  DCHECK(gl_context_);
  CHECK(eglDestroyContextFn(GetDisplay(), gl_context_));
  gl_context_ = nullptr;

  DCHECK(gl_surface_);
  CHECK(eglDestroySurfaceFn(GetDisplay(), gl_surface_));
  gl_surface_ = nullptr;
}

bool FakeHWUIGLContext::HaveContext() {
  return !!gl_context_;
}

void FakeHWUIGLContext::MakeCurrent() {
  DCHECK(gl_surface_);
  DCHECK(gl_context_);
  CHECK(eglMakeCurrentFn(GetDisplay(), gl_surface_, gl_surface_, gl_context_));
}

int FakeHWUIGLContext::ReadPixel(int x, int y) {
  GLubyte bytes[4] = {};
  glReadPixelsFn(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
  return ((bytes[3]) & 0xff) << 24 | (bytes[0] & 0xff) << 16 |
         ((bytes[1]) & 0xff) << 8 | ((bytes[2]) & 0xff);
}

void FakeHWUIGLContext::SwapBuffers() {
  CHECK(eglSwapBuffersFn(GetDisplay(), gl_surface_));
}

void FakeHWUIGLContext::InitializeGLBindings() {
  if (eglGetProcAddressFn) {
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::NativeLibraryLoadError error;
  base::FilePath filename("libEGL.so");
  base::NativeLibrary egl_library = base::LoadNativeLibrary(filename, &error);
  CHECK(egl_library) << "Failed to load " << filename.MaybeAsASCII() << ": "
                     << error.ToString();

  eglGetProcAddressFn = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(
      base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                "eglGetProcAddress"));
  CHECK(eglGetProcAddressFn) << "Failed to get eglGetProcAddress.";

  AssignProc(eglBindAPIFn, "eglBindAPI");
  AssignProc(eglInitialize, "eglInitialize");
  AssignProc(eglGetDisplayFn, "eglGetDisplay");
  AssignProc(eglMakeCurrentFn, "eglMakeCurrent");
  AssignProc(eglSwapBuffersFn, "eglSwapBuffers");
  AssignProc(eglChooseConfigFn, "eglChooseConfig");
  AssignProc(eglCreateContextFn, "eglCreateContext");
  AssignProc(eglDestroyContextFn, "eglDestroyContext");
  AssignProc(eglCreateWindowSurfaceFn, "eglCreateWindowSurface");
  AssignProc(eglDestroySurfaceFn, "eglDestroySurface");
  AssignProc(glReadPixelsFn, "glReadPixels");
}

EGLDisplay FakeHWUIGLContext::GetDisplay() {
  static EGLDisplay display = nullptr;
  if (!display) {
    display = eglGetDisplayFn(EGL_DEFAULT_DISPLAY);
    CHECK_NE(display, EGL_NO_DISPLAY);
    CHECK(eglInitialize(display, nullptr, nullptr));
  }
  return display;
}

EGLConfig FakeHWUIGLContext::GetConfig() {
  static EGLConfig config = nullptr;
  if (config) {
    return config;
  }

  EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                             32,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_RED_SIZE,
                             8,
                             EGL_SAMPLES,
                             -1,
                             EGL_DEPTH_SIZE,
                             -1,
                             EGL_STENCIL_SIZE,
                             -1,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES3_BIT,
                             EGL_SURFACE_TYPE,
                             EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                             EGL_NONE};
  EGLint num_configs = 0;
  CHECK(eglChooseConfigFn(GetDisplay(), config_attribs, nullptr, 0,
                          &num_configs));
  CHECK_GT(num_configs, 0);

  CHECK(eglChooseConfigFn(GetDisplay(), config_attribs, &config, 1,
                          &num_configs));

  CHECK(config);
  return config;
}

}  // namespace android_webview
