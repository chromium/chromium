// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/private/chromium/GrVkSecondaryCBDrawContext.h"

struct AwDrawFn_InitVkParams;
class GrDirectContext;

namespace gpu {
class VulkanImplementation;
class VulkanDeviceQueue;
}  // namespace gpu

namespace android_webview {

// Encapsulates mutable per-draw state for a secondary command buffer rendering
// pass. This state is owned by ScopedSecondaryCBDraw during recording and held
// until PostDrawVk() is invoked by Android HWUI.
struct SecondaryCBDrawState {
  sk_sp<GrVkSecondaryCBDrawContext> draw_context;
  std::vector<VkSemaphore> post_submit_semaphores;
  std::vector<base::OnceClosure> post_submit_tasks;
};

// Lifetime: WebView
class AwVulkanContextProvider final : public gpu::VulkanContextProvider {
 public:
  // Short-lived per draw pass. Created in DrawVk() and destroyed in
  // PostDrawVk().
  //
  // Manages the lifecycle of a secondary command buffer draw pass:
  // - On creation, registers this draw's SecondaryCBDrawState with the provider
  // so Viz can record into it during synchronous DrawOnRT().
  // - RecordingFinished() detaches the draw state from the provider once
  // DrawOnRT() returns, allowing subsequent WebViews to safely record on a
  // shared provider.
  // - On destruction (in PostDrawVk()), hands over the draw context,
  // semaphores, and post-submit callbacks to the provider for submission and
  // cleanup.
  class ScopedSecondaryCBDraw {
   public:
    ScopedSecondaryCBDraw(AwVulkanContextProvider* provider,
                          sk_sp<GrVkSecondaryCBDrawContext> draw_context);

    ScopedSecondaryCBDraw(ScopedSecondaryCBDraw&&);
    ScopedSecondaryCBDraw& operator=(ScopedSecondaryCBDraw&&);

    ScopedSecondaryCBDraw(const ScopedSecondaryCBDraw&) = delete;
    ScopedSecondaryCBDraw& operator=(const ScopedSecondaryCBDraw&) = delete;

    ~ScopedSecondaryCBDraw();

    void RecordingFinished();

   private:
    raw_ptr<AwVulkanContextProvider> provider_;
    SecondaryCBDrawState state_;
    bool recording_active_ = true;
  };

  AwVulkanContextProvider(const AwVulkanContextProvider&) = delete;
  AwVulkanContextProvider& operator=(const AwVulkanContextProvider&) = delete;

  static scoped_refptr<AwVulkanContextProvider> Create(
      AwDrawFn_InitVkParams* params);

  // gpu::VulkanContextProvider implementation:
  bool InitializeGrContext(const GrContextOptions& context_options) override;
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrDirectContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;
  std::optional<uint32_t> GetSyncCpuMemoryLimit() const override;

  VkDevice device() { return globals_->device_queue->GetVulkanDevice(); }
  VkQueue queue() { return globals_->device_queue->GetVulkanQueue(); }

 private:
  friend class base::RefCounted<AwVulkanContextProvider>;

  AwVulkanContextProvider();
  ~AwVulkanContextProvider() override;

  bool Initialize(AwDrawFn_InitVkParams* params);
  void SecondaryCBDrawBegin(SecondaryCBDrawState* state);
  void SecondaryCBDrawRecordingFinished(SecondaryCBDrawState* state);
  void SecondaryCBDrawSubmitted(SecondaryCBDrawState state);

  // Lifetime: Singleton
  //
  // This counts its number of active users and will spin up and tear down
  // according to demand. As such, it may not be the same singleton throughout
  // the process's lifetime.
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
  // Temporary pointer to the SecondaryCBDrawState of the WebView currently
  // executing synchronous Viz recording in DrawOnRT(). Non-null strictly during
  // DrawOnRT() on the RenderThread.
  raw_ptr<SecondaryCBDrawState> active_draw_state_ = nullptr;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_VULKAN_CONTEXT_PROVIDER_H_
