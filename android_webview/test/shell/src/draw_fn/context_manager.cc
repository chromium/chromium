// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/shell/src/draw_fn/context_manager.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window_jni.h>

#include "android_webview/public/browser/draw_fn.h"
#include "android_webview/test/draw_fn_impl_jni_headers/ContextManager_jni.h"
#include "android_webview/test/shell/src/draw_fn/allocator.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"

namespace draw_fn {

namespace {

// TODO(penghuang): remove those proc types when EGL header is updated to 1.5.
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

PFNEGLGETPROCADDRESSPROC eglGetProcAddressFn = nullptr;
PFNEGLBINDAPIPROC eglBindAPIFn = nullptr;
PFNEGLGETDISPLAYPROC eglGetDisplayFn = nullptr;
PFNEGLMAKECURRENTPROC eglMakeCurrentFn = nullptr;
PFNEGLSWAPBUFFERSPROC eglSwapBuffersFn = nullptr;
PFNEGLCHOOSECONFIGPROC eglChooseConfigFn = nullptr;
PFNEGLCREATECONTEXTPROC eglCreateContextFn = nullptr;
PFNEGLDESTROYCONTEXTPROC eglDestroyContextFn = nullptr;
PFNEGLCREATEWINDOWSURFACEPROC eglCreateWindowSurfaceFn = nullptr;
PFNEGLDESTROYSURFACEPROC eglDestroySurfaceFn = nullptr;
PFNGLREADPIXELSPROC glReadPixelsFn = nullptr;

template <typename T>
void AssignProc(T& fn, const char* name) {
  fn = reinterpret_cast<T>(eglGetProcAddressFn(name));
  CHECK(fn) << "Failed to get " << name;
}

void InitializeBindings() {
  if (eglGetProcAddressFn)
    return;

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

}  // namespace

static jlong JNI_ContextManager_GetDrawFnFunctionTable(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(draw_fn::GetDrawFnFunctionTable());
}

static jlong JNI_ContextManager_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new ContextManager);
}

ContextManager::ContextManager() {
  InitializeBindings();
}

ContextManager::~ContextManager() {
  DestroyContext();
}

void ContextManager::SetSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface) {
  if (!java_surface_.is_null()) {
    DestroyContext();
  }
  if (!surface.is_null()) {
    CreateContext(env, surface);
  }
}

void ContextManager::Sync(JNIEnv* env, int functor, bool apply_force_dark) {
  if (current_functor_ && current_functor_ != functor) {
    FunctorData data = Allocator::Get()->get(current_functor_);
    data.functor_callbacks->on_context_destroyed(data.functor, data.data);
    Allocator::Get()->MarkReleasedByManager(current_functor_);
  }
  current_functor_ = functor;

  FunctorData data = Allocator::Get()->get(current_functor_);
  AwDrawFn_OnSyncParams params{kAwDrawFnVersion, apply_force_dark};
  data.functor_callbacks->on_sync(current_functor_, data.data, &params);
}

namespace {

ASurfaceControl* GetSurfaceControl() {
  NOTREACHED();
  return nullptr;
}

void MergeTransaction(ASurfaceTransaction* transaction) {
  NOTREACHED();
}

EGLDisplay GetDisplay() {
  static EGLDisplay display = eglGetDisplayFn(EGL_DEFAULT_DISPLAY);
  CHECK_NE(display, EGL_NO_DISPLAY);
  return display;
}

int rgbaToArgb(GLubyte* bytes) {
  return (bytes[3] & 0xff) << 24 | (bytes[0] & 0xff) << 16 |
         (bytes[1] & 0xff) << 8 | (bytes[2] & 0xff);
}

}  // namespace

base::android::ScopedJavaLocalRef<jintArray> ContextManager::Draw(
    JNIEnv* env,
    int width,
    int height,
    int scroll_x,
    int scroll_y,
    jboolean readback_quadrants) {
  int results[] = {0, 0, 0, 0};
  if (!context_ || !current_functor_) {
    LOG(ERROR) << "Draw failed. context:" << context_
               << " functor:" << current_functor_;
    return readback_quadrants ? base::android::ToJavaIntArray(env, results)
                              : nullptr;
  }

  MakeCurrent();

  FunctorData data = Allocator::Get()->get(current_functor_);
  AwDrawFn_DrawGLParams params{kAwDrawFnVersion};
  params.width = width;
  params.height = height;
  params.clip_left = 0;
  params.clip_top = 0;
  params.clip_bottom = height;
  params.clip_right = width;
  params.transform[0] = 1.0;
  params.transform[1] = 0.0;
  params.transform[2] = 0.0;
  params.transform[3] = 0.0;

  params.transform[4] = 0.0;
  params.transform[5] = 1.0;
  params.transform[6] = 0.0;
  params.transform[7] = 0.0;

  params.transform[8] = 0.0;
  params.transform[9] = 0.0;
  params.transform[10] = 1.0;
  params.transform[11] = 0.0;

  params.transform[12] = -scroll_x;
  params.transform[13] = -scroll_y;
  params.transform[14] = 0.0;
  params.transform[15] = 1.0;

  // Hard coded value for sRGB.
  params.transfer_function_g = 2.4f;
  params.transfer_function_a = 0.947867f;
  params.transfer_function_b = 0.0521327f;
  params.transfer_function_c = 0.0773994f;
  params.transfer_function_d = 0.0404499f;
  params.transfer_function_e = 0.f;
  params.transfer_function_f = 0.f;
  params.color_space_toXYZD50[0] = 0.436028f;
  params.color_space_toXYZD50[1] = 0.385101f;
  params.color_space_toXYZD50[2] = 0.143091f;
  params.color_space_toXYZD50[3] = 0.222479f;
  params.color_space_toXYZD50[4] = 0.716897f;
  params.color_space_toXYZD50[5] = 0.0606241f;
  params.color_space_toXYZD50[6] = 0.0139264f;
  params.color_space_toXYZD50[7] = 0.0970921f;
  params.color_space_toXYZD50[8] = 0.714191;

  params.overlays_mode = AW_DRAW_FN_OVERLAYS_MODE_DISABLED;
  params.get_surface_control = GetSurfaceControl;
  params.merge_transaction = MergeTransaction;

  data.functor_callbacks->draw_gl(current_functor_, data.data, &params);

  if (readback_quadrants) {
    int quarter_width = width / 4;
    int quarter_height = height / 4;
    GLubyte bytes[4] = {};
    glReadPixelsFn(quarter_width, quarter_height * 3, 1, 1, GL_RGBA,
                   GL_UNSIGNED_BYTE, bytes);
    results[0] = rgbaToArgb(bytes);
    glReadPixelsFn(quarter_width * 3, quarter_height * 3, 1, 1, GL_RGBA,
                   GL_UNSIGNED_BYTE, bytes);
    results[1] = rgbaToArgb(bytes);
    glReadPixelsFn(quarter_width, quarter_height, 1, 1, GL_RGBA,
                   GL_UNSIGNED_BYTE, bytes);
    results[2] = rgbaToArgb(bytes);
    glReadPixelsFn(quarter_width * 3, quarter_height, 1, 1, GL_RGBA,
                   GL_UNSIGNED_BYTE, bytes);
    results[3] = rgbaToArgb(bytes);
  }

  CHECK(eglSwapBuffersFn(GetDisplay(), surface_));

  return readback_quadrants ? base::android::ToJavaIntArray(env, results)
                            : nullptr;
}

namespace {

EGLConfig GetConfig(bool* out_use_es3) {
  static EGLConfig config = nullptr;
  static bool use_es3 = false;
  if (config) {
    *out_use_es3 = use_es3;
    return config;
  }

  for (bool try_es3 : std::vector<bool>{true, false}) {
    EGLint config_attribs[] = {
        EGL_BUFFER_SIZE,
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
        try_es3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_NONE};
    EGLint num_configs = 0;
    if (!eglChooseConfigFn(GetDisplay(), config_attribs, nullptr, 0,
                           &num_configs) ||
        num_configs == 0) {
      continue;
    }

    CHECK(eglChooseConfigFn(GetDisplay(), config_attribs, &config, 1,
                            &num_configs));
    use_es3 = try_es3;
    break;
  }

  CHECK(config);
  *out_use_es3 = use_es3;
  return config;
}
}  // namespace

void ContextManager::CreateContext(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface) {
  java_surface_.Reset(surface);
  if (java_surface_.is_null())
    return;

  native_window_ = ANativeWindow_fromSurface(env, surface.obj());
  CHECK(native_window_);

  bool use_es3 = false;
  {
    std::vector<EGLint> egl_window_attributes;
    egl_window_attributes.push_back(EGL_NONE);
    surface_ =
        eglCreateWindowSurfaceFn(GetDisplay(), GetConfig(&use_es3),
                                 native_window_, &egl_window_attributes[0]);
    CHECK(surface_);
  }

  {
    std::vector<EGLint> context_attributes;
    context_attributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
    context_attributes.push_back(use_es3 ? 3 : 2);
    context_attributes.push_back(EGL_NONE);

    CHECK(eglBindAPIFn(EGL_OPENGL_ES_API));

    context_ = eglCreateContextFn(GetDisplay(), GetConfig(&use_es3), nullptr,
                                  context_attributes.data());
    CHECK(context_);
  }
}

void ContextManager::DestroyContext() {
  if (java_surface_.is_null())
    return;

  if (current_functor_) {
    MakeCurrent();
    FunctorData data = Allocator::Get()->get(current_functor_);
    data.functor_callbacks->on_context_destroyed(data.functor, data.data);
  }

  DCHECK(context_);
  CHECK(eglDestroyContextFn(GetDisplay(), context_));
  context_ = nullptr;

  DCHECK(surface_);
  CHECK(eglDestroySurfaceFn(GetDisplay(), surface_));
  surface_ = nullptr;
  ANativeWindow_release(native_window_);
  java_surface_.Reset();
}

void ContextManager::MakeCurrent() {
  DCHECK(surface_);
  DCHECK(context_);
  CHECK(eglMakeCurrentFn(GetDisplay(), surface_, surface_, context_));
}

}  // namespace draw_fn
