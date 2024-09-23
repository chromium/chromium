// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_

#include <optional>

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/scoped_java_ref.h"
#include "base/threading/platform_thread.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace android_webview {

// Lifetime: WebView
class AwDrawFnImpl {
 public:
  // Safe to call even on versions where draw_fn functor is not supported.
  static bool IsUsingVulkan();

  static void ReportRenderingThreads(int functor,
                                     const pid_t* thread_ids,
                                     size_t size);

  AwDrawFnImpl();

  AwDrawFnImpl(const AwDrawFnImpl&) = delete;
  AwDrawFnImpl& operator=(const AwDrawFnImpl&) = delete;

  ~AwDrawFnImpl();

  void ReleaseHandle(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jint GetFunctorHandle(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jlong GetCompositorFrameConsumer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int functor_handle() { return functor_handle_; }
  void OnSync(AwDrawFn_OnSyncParams* params);
  void OnContextDestroyed();
  void DrawGL(AwDrawFn_DrawGLParams* params);
  void InitVk(AwDrawFn_InitVkParams* params);
  void DrawVk(AwDrawFn_DrawVkParams* params);
  void PostDrawVk(AwDrawFn_PostDrawVkParams* params);
  void RemoveOverlays(AwDrawFn_RemoveOverlaysParams* params);

 private:
  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  int functor_handle_;

  RenderThreadManager render_thread_manager_;

  // Vulkan context provider for Vk rendering.
  scoped_refptr<AwVulkanContextProvider> vulkan_context_provider_;

  std::optional<AwVulkanContextProvider::ScopedSecondaryCBDraw>
      scoped_secondary_cb_draw_;

  // Latched on first DrawGL / InitVk call.
  std::optional<base::PlatformThreadId> render_thread_id_;

  bool skip_next_post_draw_vk_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
