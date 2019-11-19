// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_FUNCTOR_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_FUNCTOR_H_

#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/android/jni_weak_ref.h"

struct AwDrawGLInfo;

namespace android_webview {

class AwGLFunctor {
 public:
  explicit AwGLFunctor(const JavaObjectWeakGlobalRef& java_ref);
  ~AwGLFunctor();

  void Destroy(JNIEnv* env);
  void DeleteHardwareRenderer(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void RemoveFromCompositorFrameProducer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jlong GetCompositorFrameConsumer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jlong GetAwDrawGLFunction(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  void DrawGL(AwDrawGLInfo* draw_info);

 private:
  bool RequestInvokeGL(bool wait_for_completion);
  void DetachFunctorFromView();
  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  JavaObjectWeakGlobalRef java_ref_;
  RenderThreadManager render_thread_manager_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_GL_FUNCTOR_H_
