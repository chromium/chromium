// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_OVERLAYS_MANAGER_H_
#define ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_OVERLAYS_MANAGER_H_

#include <android/native_window.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/android/android_surface_control_compat.h"

struct AwDrawFn_DrawGLParams;
struct AwDrawFn_DrawVkParams;

namespace draw_fn {

struct FunctorData;

class OverlaysManager {
 public:
  class ScopedCurrentFunctorCall;
  class ScopedDraw {
   public:
    ScopedDraw(OverlaysManager& manager,
               FunctorData& functor,
               AwDrawFn_DrawGLParams& params);
    ScopedDraw(OverlaysManager& manager,
               FunctorData& functor,
               AwDrawFn_DrawVkParams& params);
    ~ScopedDraw();

   private:
    std::unique_ptr<ScopedCurrentFunctorCall> scoped_functor_call_;
  };

  OverlaysManager();
  ~OverlaysManager();

  void RemoveOverlays(FunctorData& functor);
  void SetSurface(int current_functor,
                  JNIEnv* env,
                  const base::android::JavaRef<jobject>& surface);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_surface_;
  raw_ptr<ANativeWindow> native_window_ = nullptr;
};

}  // namespace draw_fn
#endif  // ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_OVERLAYS_MANAGER_H_
