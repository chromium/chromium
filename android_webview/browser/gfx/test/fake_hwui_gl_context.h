// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_HWUI_GL_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_HWUI_GL_CONTEXT_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "base/check.h"
#include "base/logging.h"

struct ANativeWindow;

namespace android_webview {

class FakeHWUIGLContext {
 public:
  FakeHWUIGLContext();

  FakeHWUIGLContext(const FakeHWUIGLContext&) = delete;
  FakeHWUIGLContext& operator=(const FakeHWUIGLContext&) = delete;

  ~FakeHWUIGLContext();

  void CreateWindowContext(ANativeWindow* a_native_window);
  void CreateOffscreenContext(int width, int height);
  void DestroyContext();
  bool HaveContext();
  void MakeCurrent();
  int ReadPixel(int x, int y);
  void SwapBuffers();

 private:
  // TODO(penghuang): remove those proc types when EGL header is updated to 1.5.
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLINITIALIZEPROC)(EGLDisplay dpy,
                                                        EGLint* major,
                                                        EGLint* minor);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLCHOOSECONFIGPROC)(
      EGLDisplay dpy,
      const EGLint* attrib_list,
      EGLConfig* configs,
      EGLint config_size,
      EGLint* num_config);
  typedef EGLContext(EGLAPIENTRYP PFNEGLCREATECONTEXTPROC)(
      EGLDisplay dpy,
      EGLConfig config,
      EGLContext share_context,
      const EGLint* attrib_list);
  typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEWINDOWSURFACEPROC)(
      EGLDisplay dpy,
      EGLConfig config,
      EGLNativeWindowType win,
      const EGLint* attrib_list);
  typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPBUFFERSURFACEPROC)(
      EGLDisplay dpy,
      EGLConfig config,
      const EGLint* attrib_list);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYCONTEXTPROC)(EGLDisplay dpy,
                                                            EGLContext ctx);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSURFACEPROC)(EGLDisplay dpy,
                                                            EGLSurface surface);
  typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETDISPLAYPROC)(
      EGLNativeDisplayType display_id);
  typedef __eglMustCastToProperFunctionPointerType(
      EGLAPIENTRYP PFNEGLGETPROCADDRESSPROC)(const char* procname);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLMAKECURRENTPROC)(EGLDisplay dpy,
                                                         EGLSurface draw,
                                                         EGLSurface read,
                                                         EGLContext ctx);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLSWAPBUFFERSPROC)(EGLDisplay dpy,
                                                         EGLSurface surface);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLBINDAPIPROC)(EGLenum api);

  template <typename T>
  void AssignProc(T& fn, const char* name) {
    fn = reinterpret_cast<T>(eglGetProcAddressFn(name));
    CHECK(fn) << "Failed to get " << name;
  }

  void InitializeGLBindings();
  EGLDisplay GetDisplay();
  EGLConfig GetConfig();
  void CreateContextImpl();

  PFNEGLGETPROCADDRESSPROC eglGetProcAddressFn = nullptr;
  PFNEGLBINDAPIPROC eglBindAPIFn = nullptr;
  PFNEGLINITIALIZEPROC eglInitialize = nullptr;
  PFNEGLGETDISPLAYPROC eglGetDisplayFn = nullptr;
  PFNEGLMAKECURRENTPROC eglMakeCurrentFn = nullptr;
  PFNEGLSWAPBUFFERSPROC eglSwapBuffersFn = nullptr;
  PFNEGLCHOOSECONFIGPROC eglChooseConfigFn = nullptr;
  PFNEGLCREATECONTEXTPROC eglCreateContextFn = nullptr;
  PFNEGLDESTROYCONTEXTPROC eglDestroyContextFn = nullptr;
  PFNEGLCREATEWINDOWSURFACEPROC eglCreateWindowSurfaceFn = nullptr;
  PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurfaceFn = nullptr;
  PFNEGLDESTROYSURFACEPROC eglDestroySurfaceFn = nullptr;
  PFNGLREADPIXELSPROC glReadPixelsFn = nullptr;

  EGLSurface gl_surface_ = nullptr;
  EGLContext gl_context_ = nullptr;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TEST_FAKE_HWUI_GL_CONTEXT_H_
