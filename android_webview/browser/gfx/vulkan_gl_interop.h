// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_VULKAN_GL_INTEROP_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_VULKAN_GL_INTEROP_H_

#include <memory>

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/scoped_egl_image.h"

class GrVkSecondaryCBDrawContext;
class SkColorSpace;
class SkImage;

namespace gpu {
class VulkanImage;
}

namespace android_webview {

class RenderThreadManager;
struct HardwareRendererDrawParams;
struct OverlaysParams;

// With interop mode, we will render frames on AHBs with GL api, and then draw
// AHBs with Vulkan API on the final target.
class VulkanGLInterop {
 public:
  VulkanGLInterop(RenderThreadManager* render_thread_manager,
                  AwVulkanContextProvider* vulkan_context_provider);
  ~VulkanGLInterop();

  void DrawVk(sk_sp<GrVkSecondaryCBDrawContext> draw_context,
              sk_sp<SkColorSpace> color_space,
              const HardwareRendererDrawParams& params,
              const OverlaysParams& overlays_params);
  void PostDrawVk();

  // For clean up.
  void MakeGLContextCurrentIgnoreFailure();

 private:
  class GLNonOwnedCompatibilityContext;
  static GLNonOwnedCompatibilityContext* g_gl_context;

  // Struct which represents one in-flight draw for the Vk interop path.
  struct InFlightInteropDraw {
    explicit InFlightInteropDraw(AwVulkanContextProvider* vk_context_provider);
    ~InFlightInteropDraw();

    sk_sp<GrVkSecondaryCBDrawContext> draw_context;
    VkFence post_draw_fence = VK_NULL_HANDLE;
    VkSemaphore post_draw_semaphore = VK_NULL_HANDLE;
    base::ScopedFD sync_fd;
    gl::ScopedEGLImage egl_image;
    gfx::Size image_size;
    base::android::ScopedHardwareBufferHandle scoped_buffer;
    sk_sp<SkImage> ahb_skimage;
    uint32_t texture_id = 0;
    uint32_t framebuffer_id = 0;
    std::unique_ptr<gpu::VulkanImage> vulkan_image;
    GrVkImageInfo image_info;

    // Used to clean up Vulkan objects.
    raw_ptr<AwVulkanContextProvider> vk_context_provider;
  };

  RenderThreadManager* const render_thread_manager_;
  AwVulkanContextProvider* const vulkan_context_provider_;

  // GL context used to draw via GL in Vk interop path.
  scoped_refptr<GLNonOwnedCompatibilityContext> gl_context_;

  // Queue of draw contexts pending cleanup.
  base::queue<std::unique_ptr<InFlightInteropDraw>> in_flight_interop_draws_;
  std::unique_ptr<InFlightInteropDraw> pending_draw_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_VULKAN_GL_INTEROP_H_
