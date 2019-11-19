// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"

#include <utility>

#include "android_webview/public/browser/draw_fn.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"

namespace android_webview {

namespace {

AwVulkanContextProvider* g_vulkan_context_provider = nullptr;

GrVkGetProc MakeUnifiedGetter(const PFN_vkGetInstanceProcAddr& iproc,
                              const PFN_vkGetDeviceProcAddr& dproc) {
  return [&iproc, &dproc](const char* proc_name, VkInstance instance,
                          VkDevice device) {
    if (device != VK_NULL_HANDLE) {
      return dproc(device, proc_name);
    }
    return iproc(instance, proc_name);
  };
}

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
  if (!vulkan_function_pointers->vulkan_loader_library_) {
    base::NativeLibraryLoadError native_library_load_error;
    vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
        base::FilePath("libvulkan.so"), &native_library_load_error);
    if (!vulkan_function_pointers->vulkan_loader_library_)
      return false;
    if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
      return false;
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
scoped_refptr<AwVulkanContextProvider>
AwVulkanContextProvider::GetOrCreateInstance(AwDrawFn_InitVkParams* params) {
  if (g_vulkan_context_provider) {
    DCHECK(!params || params->device == g_vulkan_context_provider->device());
    DCHECK(!params || params->queue == g_vulkan_context_provider->queue());
    return base::WrapRefCounted(g_vulkan_context_provider);
  }

  auto provider = base::WrapRefCounted(new AwVulkanContextProvider);
  if (!provider->Initialize(params))
    return nullptr;

  return provider;
}

AwVulkanContextProvider::AwVulkanContextProvider() {
  DCHECK_EQ(nullptr, g_vulkan_context_provider);
  g_vulkan_context_provider = this;
}

AwVulkanContextProvider::~AwVulkanContextProvider() {
  DCHECK_EQ(g_vulkan_context_provider, this);
  g_vulkan_context_provider = nullptr;
  device_queue_->Destroy();
  device_queue_ = nullptr;
}

gpu::VulkanImplementation* AwVulkanContextProvider::GetVulkanImplementation() {
  return implementation_.get();
}

gpu::VulkanDeviceQueue* AwVulkanContextProvider::GetDeviceQueue() {
  return device_queue_.get();
}

GrContext* AwVulkanContextProvider::GetGrContext() {
  return gr_context_.get();
}

GrVkSecondaryCBDrawContext*
AwVulkanContextProvider::GetGrSecondaryCBDrawContext() {
  return draw_context_.get();
}

void AwVulkanContextProvider::EnqueueSecondaryCBSemaphores(
    std::vector<VkSemaphore> semaphores) {
  post_submit_semaphores_.reserve(post_submit_semaphores_.size() +
                                  semaphores.size());
  std::copy(semaphores.begin(), semaphores.end(),
            std::back_inserter(post_submit_semaphores_));
}

void AwVulkanContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  post_submit_tasks_.push_back(std::move(closure));
}

bool AwVulkanContextProvider::Initialize(AwDrawFn_InitVkParams* params) {
  DCHECK(params);
  // Don't call init on implementation. Instead call InitVulkanForWebView,
  // which avoids creating a new instance.
  implementation_ = gpu::CreateVulkanImplementation();

  gfx::ExtensionSet instance_extensions;
  for (uint32_t i = 0; i < params->enabled_instance_extension_names_length; ++i)
    instance_extensions.insert(params->enabled_instance_extension_names[i]);

  gfx::ExtensionSet device_extensions;
  for (uint32_t i = 0; i < params->enabled_device_extension_names_length; ++i)
    device_extensions.insert(params->enabled_device_extension_names[i]);

  if (!InitVulkanForWebView(params->instance, params->physical_device,
                            params->device, params->api_version,
                            instance_extensions, device_extensions)) {
    LOG(ERROR) << "Unable to initialize Vulkan pointers.";
    return false;
  }

  device_queue_ = std::make_unique<gpu::VulkanDeviceQueue>(params->instance);
  device_queue_->InitializeForWebView(
      params->physical_device, params->device, params->queue,
      params->graphics_queue_index, std::move(device_extensions));

  // Create our Skia GrContext.
  GrVkGetProc get_proc =
      MakeUnifiedGetter(vkGetInstanceProcAddr, vkGetDeviceProcAddr);
  GrVkExtensions vk_extensions;
  vk_extensions.init(get_proc, params->instance, params->physical_device,
                     params->enabled_instance_extension_names_length,
                     params->enabled_instance_extension_names,
                     params->enabled_device_extension_names_length,
                     params->enabled_device_extension_names);
  GrVkBackendContext backend_context{
      .fInstance = params->instance,
      .fPhysicalDevice = params->physical_device,
      .fDevice = params->device,
      .fQueue = params->queue,
      .fGraphicsQueueIndex = params->graphics_queue_index,
      .fMaxAPIVersion = params->api_version,
      .fVkExtensions = &vk_extensions,
      .fDeviceFeatures = params->device_features,
      .fDeviceFeatures2 = params->device_features_2,
      .fMemoryAllocator = nullptr,
      .fGetProc = get_proc,
      .fOwnsInstanceAndDevice = false,
  };
  gr_context_ = GrContext::MakeVulkan(backend_context);
  if (!gr_context_) {
    LOG(ERROR) << "Unable to initialize GrContext.";
    return false;
  }
  return true;
}

void AwVulkanContextProvider::SecondaryCBDrawBegin(
    sk_sp<GrVkSecondaryCBDrawContext> draw_context) {
  DCHECK(draw_context);
  DCHECK(!draw_context_);
  DCHECK(post_submit_tasks_.empty());
  draw_context_ = draw_context;
}

void AwVulkanContextProvider::SecondaryCMBDrawSubmitted() {
  DCHECK(draw_context_);
  auto draw_context = std::move(draw_context_);

  auto* fence_helper = device_queue_->GetFenceHelper();
  VkFence vk_fence = VK_NULL_HANDLE;
  auto result = fence_helper->GetFence(&vk_fence);
  DCHECK(result == VK_SUCCESS);
  gpu::SubmitSignalVkSemaphores(queue(), post_submit_semaphores_, vk_fence);

  post_submit_semaphores_.clear();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](sk_sp<GrVkSecondaryCBDrawContext> context,
         gpu::VulkanDeviceQueue* device_queue, bool device_lost) {
        context->releaseResources();
        DCHECK(context->unique());
        context = nullptr;
      },
      std::move(draw_context)));
  for (auto& closure : post_submit_tasks_)
    std::move(closure).Run();
  post_submit_tasks_.clear();

  fence_helper->EnqueueFence(vk_fence);
  fence_helper->ProcessCleanupTasks();
}

}  // namespace android_webview
