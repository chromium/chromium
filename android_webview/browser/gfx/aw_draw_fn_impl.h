// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/vulkan_gl_interop.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/scoped_java_ref.h"
#include "base/threading/platform_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace android_webview {

class AwDrawFnImpl {
 public:
  // Safe to call even on versions where draw_fn functor is not supported.
  static bool IsUsingVulkan();

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
  // With direct mode, we will render frames with Vulkan API directly.
  void DrawVkDirect(sk_sp<GrVkSecondaryCBDrawContext> draw_context,
                    sk_sp<SkColorSpace> color_space,
                    const HardwareRendererDrawParams& params,
                    const OverlaysParams& overlays_params);
  void PostDrawVkDirect(AwDrawFn_PostDrawVkParams* params);

  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  const bool is_interop_mode_;

  int functor_handle_;

  RenderThreadManager render_thread_manager_;

  // Vulkan context provider for Vk rendering.
  scoped_refptr<AwVulkanContextProvider> vulkan_context_provider_;

  absl::optional<AwVulkanContextProvider::ScopedSecondaryCBDraw>
      scoped_secondary_cb_draw_;

  absl::optional<VulkanGLInterop> interop_;

  // Latched on first DrawGL / InitVk call.
  absl::optional<base::PlatformThreadId> render_thread_id_;

  bool skip_next_post_draw_vk_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
