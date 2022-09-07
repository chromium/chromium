// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_

#include <memory>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"

struct AwDrawFn_InitVkParams;
class GrDirectContext;
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

    ScopedSecondaryCBDraw(const ScopedSecondaryCBDraw&) = delete;
    ScopedSecondaryCBDraw& operator=(const ScopedSecondaryCBDraw&) = delete;

    ~ScopedSecondaryCBDraw() { provider_->SecondaryCMBDrawSubmitted(); }

   private:
    AwVulkanContextProvider* const provider_;
  };

  AwVulkanContextProvider(const AwVulkanContextProvider&) = delete;
  AwVulkanContextProvider& operator=(const AwVulkanContextProvider&) = delete;

  static scoped_refptr<AwVulkanContextProvider> Create(
      AwDrawFn_InitVkParams* params);

  // viz::VulkanContextProvider implementation:
  bool InitializeGrContext(const GrContextOptions& context_options) override;
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrDirectContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;
  absl::optional<uint32_t> GetSyncCpuMemoryLimit() const override;

  VkDevice device() { return globals_->device_queue->GetVulkanDevice(); }
  VkQueue queue() { return globals_->device_queue->GetVulkanQueue(); }

 private:
  friend class base::RefCounted<AwVulkanContextProvider>;

  AwVulkanContextProvider();
  ~AwVulkanContextProvider() override;

  bool Initialize(AwDrawFn_InitVkParams* params);
  void SecondaryCBDrawBegin(sk_sp<GrVkSecondaryCBDrawContext> draw_context);
  void SecondaryCMBDrawSubmitted();

  struct Globals : base::RefCountedThreadSafe<Globals> {
    static scoped_refptr<Globals> GetOrCreateInstance(
        AwDrawFn_InitVkParams* params);

    Globals();
    bool Initialize(AwDrawFn_InitVkParams* params);

    std::unique_ptr<gpu::VulkanImplementation> implementation;
    std::unique_ptr<gpu::VulkanDeviceQueue> device_queue;
    sk_sp<GrDirectContext> gr_context;

   private:
    friend base::RefCountedThreadSafe<Globals>;
    ~Globals();
  };
  static Globals* g_globals;

  scoped_refptr<Globals> globals_;
  sk_sp<GrVkSecondaryCBDrawContext> draw_context_;
  std::vector<base::OnceClosure> post_submit_tasks_;
  std::vector<VkSemaphore> post_submit_semaphores_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
