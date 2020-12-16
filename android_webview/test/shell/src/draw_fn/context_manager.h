// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_
#define ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_

#include <android/native_window.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"

typedef void* EGLContext;
typedef void* EGLSurface;

namespace draw_fn {

class ContextManager {
 public:
  ContextManager();
  ~ContextManager();

  void SetSurface(JNIEnv* env, const base::android::JavaRef<jobject>& surface);
  void Sync(JNIEnv* env, int functor, bool apply_force_dark);
  base::android::ScopedJavaLocalRef<jintArray> Draw(
      JNIEnv* env,
      int width,
      int height,
      int scroll_x,
      int scroll_y,
      jboolean readback_quadrants);

 private:
  void CreateContext(JNIEnv* env,
                     const base::android::JavaRef<jobject>& surface);
  void DestroyContext();
  void MakeCurrent();

  base::android::ScopedJavaGlobalRef<jobject> java_surface_;
  ANativeWindow* native_window_ = nullptr;
  EGLSurface surface_ = nullptr;
  EGLContext context_ = nullptr;

  int current_functor_ = 0;
};

}  // namespace draw_fn

#endif  // ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_CONTEXT_MANAGER_H_
