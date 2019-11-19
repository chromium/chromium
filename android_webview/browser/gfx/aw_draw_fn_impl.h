// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_

#include <memory>

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

class GrVkSecondaryCBDrawContext;

namespace gl {
class GLImageAHardwareBuffer;
}

namespace android_webview {
class GLNonOwnedCompatibilityContext;

class AwDrawFnImpl {
 public:
  AwDrawFnImpl();
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

 private:
  // With direct mode, we will render frames with Vulkan API directly.
  void DrawVkDirect(AwDrawFn_DrawVkParams* params);
  void PostDrawVkDirect(AwDrawFn_PostDrawVkParams* params);

  // With interop mode, we will render frames on AHBs with GL api, and then draw
  // AHBs with Vulkan API on the final target.
  void DrawVkInterop(AwDrawFn_DrawVkParams* params);
  void PostDrawVkInterop(AwDrawFn_PostDrawVkParams* params);

  template <typename T>
  void DrawInternal(T* params, SkColorSpace* color_space);

  // Struct which represents one in-flight draw for the Vk interop path.
  struct InFlightInteropDraw {
    explicit InFlightInteropDraw(AwVulkanContextProvider* vk_context_provider);
    ~InFlightInteropDraw();
    sk_sp<GrVkSecondaryCBDrawContext> draw_context;
    VkFence post_draw_fence = VK_NULL_HANDLE;
    VkSemaphore post_draw_semaphore = VK_NULL_HANDLE;
    base::ScopedFD sync_fd;
    scoped_refptr<gl::GLImageAHardwareBuffer> ahb_image;
    sk_sp<SkImage> ahb_skimage;
    uint32_t texture_id = 0;
    uint32_t framebuffer_id = 0;
    GrVkImageInfo image_info;

    // Used to clean up Vulkan objects.
    AwVulkanContextProvider* vk_context_provider;
  };

  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  const bool is_interop_mode_;

  int functor_handle_;

  RenderThreadManager render_thread_manager_;

  // Vulkan context provider for Vk rendering.
  scoped_refptr<AwVulkanContextProvider> vulkan_context_provider_;

  base::Optional<AwVulkanContextProvider::ScopedSecondaryCBDraw>
      scoped_secondary_cb_draw_;

  // GL context used to draw via GL in Vk interop path.
  scoped_refptr<GLNonOwnedCompatibilityContext> gl_context_;

  // Queue of draw contexts pending cleanup.
  base::queue<std::unique_ptr<InFlightInteropDraw>> in_flight_interop_draws_;
  std::unique_ptr<InFlightInteropDraw> pending_draw_;

  DISALLOW_COPY_AND_ASSIGN(AwDrawFnImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_DRAW_FN_IMPL_H_
