// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"

struct AwDrawFn_InitVkParams;
class GrContext;
class GrVkSecondaryCBDrawContext;

namespace gpu {
class VulkanImplementation;
class VulkanDeviceQueue;
}  // namespace gpu

namespace android_webview {

class AwVulkanContextProvider final : public viz::VulkanContextProvider {
 public:
  class ScopedSecondaryCBDraw {
   public:
    ScopedSecondaryCBDraw(AwVulkanContextProvider* provider,
                          sk_sp<GrVkSecondaryCBDrawContext> draw_context)
        : provider_(provider) {
      provider_->SecondaryCBDrawBegin(std::move(draw_context));
    }
    ~ScopedSecondaryCBDraw() { provider_->SecondaryCMBDrawSubmitted(); }

   private:
    AwVulkanContextProvider* const provider_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSecondaryCBDraw);
  };

  static scoped_refptr<AwVulkanContextProvider> GetOrCreateInstance(
      AwDrawFn_InitVkParams* params = nullptr);

  // viz::VulkanContextProvider implementation:
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;

  VkPhysicalDevice physical_device() {
    return device_queue_->GetVulkanPhysicalDevice();
  }
  VkDevice device() { return device_queue_->GetVulkanDevice(); }
  VkQueue queue() { return device_queue_->GetVulkanQueue(); }
  gpu::VulkanImplementation* implementation() { return implementation_.get(); }
  GrContext* gr_context() { return gr_context_.get(); }

 private:
  friend class base::RefCounted<AwVulkanContextProvider>;

  AwVulkanContextProvider();
  ~AwVulkanContextProvider() override;

  bool Initialize(AwDrawFn_InitVkParams* params);
  void SecondaryCBDrawBegin(sk_sp<GrVkSecondaryCBDrawContext> draw_context);
  void SecondaryCMBDrawSubmitted();

  std::unique_ptr<gpu::VulkanImplementation> implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  sk_sp<GrContext> gr_context_;
  sk_sp<GrVkSecondaryCBDrawContext> draw_context_;
  std::vector<base::OnceClosure> post_submit_tasks_;
  std::vector<VkSemaphore> post_submit_semaphores_;

  DISALLOW_COPY_AND_ASSIGN(AwVulkanContextProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
