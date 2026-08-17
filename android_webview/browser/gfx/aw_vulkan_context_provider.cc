// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"

#include <algorithm>
#include <utility>

#include "android_webview/common/aw_features.h"
#include "android_webview/common/gfx/aw_gr_context_options_provider.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "gpu/config/skia_limits.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/skia_vk_memory_allocator_impl.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "third_party/skia/include/gpu/vk/VulkanBackendContext.h"
#include "third_party/skia/include/gpu/vk/VulkanExtensions.h"
#include "third_party/skia/include/gpu/vk/VulkanTypes.h"

namespace android_webview {

AwVulkanContextProvider::Globals* AwVulkanContextProvider::g_globals = nullptr;

namespace {

AwVulkanContextProvider* g_shared_provider = nullptr;

bool InitVulkanForWebView(VkInstance instance,
                          VkPhysicalDevice physical_device,
                          VkDevice device,
                          uint32_t api_version,
                          gfx::ExtensionSet instance_extensions,
                          gfx::ExtensionSet device_extensions) {
  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  // If we are re-initing, we don't need to re-load the shared library or
  // re-bind unassociated pointers. These shouldn't change.
  if (!vulkan_function_pointers->vkGetInstanceProcAddr) {
    base::NativeLibraryLoadError native_library_load_error;
    base::NativeLibrary vulkan_loader_library = base::LoadNativeLibrary(
        base::FilePath("libvulkan.so"), &native_library_load_error);
    if (!vulkan_loader_library)
      return false;
    if (!vulkan_function_pointers
             ->BindUnassociatedFunctionPointersFromLoaderLib(
                 vulkan_loader_library)) {
      return false;
    }
  }

  // These vars depend on |instance| and |device| and should be
  // re-initialized.
  if (!vulkan_function_pointers->BindInstanceFunctionPointers(
          instance, api_version, instance_extensions)) {
    return false;
  }

  // Get API version for the selected physical device.
  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties(physical_device, &device_properties);
  uint32_t device_api_version =
      std::min(api_version, device_properties.apiVersion);

  return vulkan_function_pointers->BindDeviceFunctionPointers(
      device, device_api_version, device_extensions);
}

}  // namespace

// static
scoped_refptr<AwVulkanContextProvider::Globals>
AwVulkanContextProvider::Globals::GetOrCreateInstance(
    AwDrawFn_InitVkParams* params) {
  if (g_globals) {
    DCHECK(params->device == g_globals->device_queue->GetVulkanDevice());
    DCHECK(params->queue == g_globals->device_queue->GetVulkanQueue());
    return base::WrapRefCounted(g_globals);
  }
  auto globals = base::MakeRefCounted<AwVulkanContextProvider::Globals>();
  if (!globals->Initialize(params))
    return nullptr;
  return globals;
}

AwVulkanContextProvider::Globals::Globals() {
  DCHECK_EQ(nullptr, g_globals);
  g_globals = this;
}

AwVulkanContextProvider::Globals::~Globals() {
  DCHECK_EQ(g_globals, this);
  g_globals = nullptr;

  gr_context.reset();
  device_queue->Destroy();
  device_queue = nullptr;
}

bool AwVulkanContextProvider::Globals::Initialize(
    AwDrawFn_InitVkParams* params) {
  // Don't call init on implementation. Instead call InitVulkanForWebView,
  // which avoids creating a new instance.
  implementation = gpu::CreateVulkanImplementation();

  gfx::ExtensionSet instance_extensions;
  for (uint32_t i = 0; i < params->enabled_instance_extension_names_length; ++i)
    instance_extensions.insert(
        UNSAFE_TODO(params->enabled_instance_extension_names[i]));

  gfx::ExtensionSet device_extensions;
  for (uint32_t i = 0; i < params->enabled_device_extension_names_length; ++i)
    device_extensions.insert(
        UNSAFE_TODO(params->enabled_device_extension_names[i]));

  if (!InitVulkanForWebView(params->instance, params->physical_device,
                            params->device, params->api_version,
                            instance_extensions, device_extensions)) {
    LOG(ERROR) << "Unable to initialize Vulkan pointers.";
    return false;
  }

  device_queue = std::make_unique<gpu::VulkanDeviceQueue>(params->instance);
  device_queue->InitializeForWebView(
      params->physical_device, params->device, params->queue,
      params->graphics_queue_index, std::move(device_extensions));

  // Create our Skia GrContext.
  skgpu::VulkanGetProc get_proc = [](const char* proc_name, VkInstance instance,
                                     VkDevice device) {
    return device ? vkGetDeviceProcAddr(device, proc_name)
                  : vkGetInstanceProcAddr(instance, proc_name);
  };
  skgpu::VulkanExtensions vk_extensions;
  vk_extensions.init(get_proc, params->instance, params->physical_device,
                     params->enabled_instance_extension_names_length,
                     params->enabled_instance_extension_names,
                     params->enabled_device_extension_names_length,
                     params->enabled_device_extension_names);
  skgpu::VulkanBackendContext backend_context{
      .fInstance = params->instance,
      .fPhysicalDevice = params->physical_device,
      .fDevice = params->device,
      .fQueue = params->queue,
      .fGraphicsQueueIndex = params->graphics_queue_index,
      .fMaxAPIVersion = params->api_version,
      .fVkExtensions = &vk_extensions,
      .fDeviceFeatures = params->device_features,
      .fDeviceFeatures2 = params->device_features_2,
      .fMemoryAllocator = device_queue->GetSkiaVkMemoryAllocator(),
      .fGetProc = get_proc,
  };
  GrContextOptions options;
  std::unique_ptr<AwGrContextOptionsProvider> options_provider =
      std::make_unique<AwGrContextOptionsProvider>();
  options_provider->SetCustomGrContextOptions(options);
  gr_context = GrDirectContexts::MakeVulkan(backend_context, options);
  if (!gr_context) {
    LOG(ERROR) << "Unable to initialize GrContext.";
    return false;
  }
  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  gpu::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &glyph_cache_max_texture_bytes);
  gr_context->setResourceCacheLimit(max_resource_cache_bytes);
  return true;
}

// static
scoped_refptr<AwVulkanContextProvider> AwVulkanContextProvider::Create(
    AwDrawFn_InitVkParams* params) {
  if (base::FeatureList::IsEnabled(
          features::kWebViewSingleSharedContextState)) {
    if (g_shared_provider) {
      CHECK_EQ(params->device,
               g_shared_provider->GetDeviceQueue()->GetVulkanDevice());
      CHECK_EQ(params->queue,
               g_shared_provider->GetDeviceQueue()->GetVulkanQueue());
      return base::WrapRefCounted(g_shared_provider);
    }
  }

  auto provider = base::WrapRefCounted(new AwVulkanContextProvider);
  if (base::FeatureList::IsEnabled(
          features::kWebViewSingleSharedContextState)) {
    CHECK(!g_shared_provider);
    g_shared_provider = provider.get();
  }

  if (!provider->Initialize(params))
    return nullptr;

  return provider;
}

AwVulkanContextProvider::ScopedSecondaryCBDraw::ScopedSecondaryCBDraw(
    AwVulkanContextProvider* provider,
    sk_sp<GrVkSecondaryCBDrawContext> draw_context)
    : provider_(provider) {
  state_.draw_context = std::move(draw_context);
  // Attach this draw's state to the provider for the duration of Viz recording.
  provider_->SecondaryCBDrawBegin(&state_);
}

AwVulkanContextProvider::ScopedSecondaryCBDraw::~ScopedSecondaryCBDraw() {
  // Ensure we detach from provider if RecordingFinished() was not called yet.
  RecordingFinished();
  // Hand over accumulated semaphores, post-submit tasks, and draw context
  // to the provider for cleanup on HWUI submission.
  provider_->SecondaryCBDrawSubmitted(std::move(state_));
}

void AwVulkanContextProvider::ScopedSecondaryCBDraw::RecordingFinished() {
  if (recording_active_) {
    recording_active_ = false;
    // Detach from provider so other WebViews can record on this shared
    // provider.
    provider_->SecondaryCBDrawRecordingFinished(&state_);
  }
}

AwVulkanContextProvider::AwVulkanContextProvider() = default;

AwVulkanContextProvider::~AwVulkanContextProvider() {
  CHECK(!active_draw_state_);
  if (base::FeatureList::IsEnabled(
          features::kWebViewSingleSharedContextState)) {
    CHECK_EQ(g_shared_provider, this);
    g_shared_provider = nullptr;
  }
}

gpu::VulkanImplementation* AwVulkanContextProvider::GetVulkanImplementation() {
  return globals_->implementation.get();
}

gpu::VulkanDeviceQueue* AwVulkanContextProvider::GetDeviceQueue() {
  return globals_->device_queue.get();
}

GrDirectContext* AwVulkanContextProvider::GetGrContext() {
  return globals_->gr_context.get();
}

GrVkSecondaryCBDrawContext*
AwVulkanContextProvider::GetGrSecondaryCBDrawContext() {
  CHECK(active_draw_state_);
  return active_draw_state_->draw_context.get();
}

void AwVulkanContextProvider::EnqueueSecondaryCBSemaphores(
    std::vector<VkSemaphore> semaphores) {
  CHECK(active_draw_state_);
  active_draw_state_->post_submit_semaphores.reserve(
      active_draw_state_->post_submit_semaphores.size() + semaphores.size());
  std::ranges::copy(
      semaphores,
      std::back_inserter(active_draw_state_->post_submit_semaphores));
}

void AwVulkanContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  CHECK(active_draw_state_);
  active_draw_state_->post_submit_tasks.push_back(std::move(closure));
}

std::optional<uint32_t> AwVulkanContextProvider::GetSyncCpuMemoryLimit() const {
  return std::optional<uint32_t>();
}

bool AwVulkanContextProvider::Initialize(AwDrawFn_InitVkParams* params) {
  DCHECK(params);
  globals_ = Globals::GetOrCreateInstance(params);
  return !!globals_;
}

bool AwVulkanContextProvider::InitializeGrContext(
    const GrContextOptions& context_options) {
  // GrContext is created in Globals, so nothing to do here besides DCHECK.
  DCHECK(globals_);
  return globals_->gr_context.get() != nullptr;
}

void AwVulkanContextProvider::SecondaryCBDrawBegin(
    SecondaryCBDrawState* state) {
  CHECK(state);
  CHECK(state->draw_context);
  CHECK(!active_draw_state_);
  active_draw_state_ = state;
}

void AwVulkanContextProvider::SecondaryCBDrawRecordingFinished(
    SecondaryCBDrawState* state) {
  CHECK_EQ(active_draw_state_, state);
  active_draw_state_ = nullptr;
}

void AwVulkanContextProvider::SecondaryCBDrawSubmitted(
    SecondaryCBDrawState state) {
  CHECK(state.draw_context);

  auto* fence_helper = globals_->device_queue->GetFenceHelper();
  VkFence vk_fence = VK_NULL_HANDLE;
  auto result = fence_helper->GetFence(&vk_fence);
  CHECK_EQ(result, VK_SUCCESS);
  gpu::SubmitSignalVkSemaphores(queue(), state.post_submit_semaphores,
                                vk_fence);

  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](sk_sp<GrVkSecondaryCBDrawContext> context,
         gpu::VulkanDeviceQueue* device_queue, bool device_lost) {
        context->releaseResources();
        CHECK(context->unique());
        context = nullptr;
      },
      std::move(state.draw_context)));
  for (auto& closure : state.post_submit_tasks) {
    std::move(closure).Run();
  }

  fence_helper->EnqueueFence(vk_fence);
  fence_helper->ProcessCleanupTasks();
}

}  // namespace android_webview
