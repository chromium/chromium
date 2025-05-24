// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"

#include "base/check.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"

namespace {
const char kTrialName[] = "SkiaBackend";

// Synthetic trial group names. Groups added here should be added to finch
// service side as well.
const char kGroupNone[] = "None";
const char kGroupGaneshGL[] = "GL";
const char kGroupGaneshVulkan[] = "Vulkan";
const char kGroupGraphiteDawnVulkan[] = "GraphiteDawnVulkan";
const char kGroupGraphiteDawnMetal[] = "GraphiteDawnMetal";
const char kGroupGraphiteDawnD3D11[] = "GraphiteDawnD3D11";
const char kGroupGraphiteDawnD3D12[] = "GraphiteDawnD3D12";
const char kGroupGraphiteMetal[] = "GraphiteMetal";

}  // namespace

ChromeBrowserMainExtraPartsGpu::ChromeBrowserMainExtraPartsGpu() = default;

ChromeBrowserMainExtraPartsGpu::~ChromeBrowserMainExtraPartsGpu() {
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsGpu::PreCreateThreads() {
  // This should be the first time to get an instance of GpuDataManager.
  // This is where it's initialized.
  // 1) Need to initialize in-process GpuDataManager before creating threads.
  // It's unsafe to append the gpu command line switches to the global
  // CommandLine::ForCurrentProcess object after threads are created.
  // 2) Must be after other parts' PreCreateThreads to pick up chrome://flags.
  DCHECK(!content::GpuDataManager::Initialized());
  content::GpuDataManager* manager = content::GpuDataManager::GetInstance();
  manager->AddObserver(this);
}

void ChromeBrowserMainExtraPartsGpu::OnGpuInfoUpdate() {
  const auto* backend_name = GetSkiaBackendName();
  if (backend_name) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(kTrialName,
                                                              backend_name);
  }
}

const char* ChromeBrowserMainExtraPartsGpu::GetSkiaBackendName() const {
  auto* manager = content::GpuDataManager::GetInstance();
  if (!manager->IsEssentialGpuInfoAvailable()) {
    return nullptr;
  }
  switch (manager->GetGPUInfo().skia_backend_type) {
    case gpu::SkiaBackendType::kNone:
      return kGroupNone;
    case gpu::SkiaBackendType::kGaneshGL:
      return kGroupGaneshGL;
    case gpu::SkiaBackendType::kGaneshVulkan:
      return kGroupGaneshVulkan;
    case gpu::SkiaBackendType::kGraphiteDawnVulkan:
      return kGroupGraphiteDawnVulkan;
    case gpu::SkiaBackendType::kGraphiteDawnMetal:
      return kGroupGraphiteDawnMetal;
    case gpu::SkiaBackendType::kGraphiteDawnD3D11:
      return kGroupGraphiteDawnD3D11;
    case gpu::SkiaBackendType::kGraphiteDawnD3D12:
      return kGroupGraphiteDawnD3D12;
    case gpu::SkiaBackendType::kGraphiteMetal:
      return kGroupGraphiteMetal;
    case gpu::SkiaBackendType::kUnknown:
      return nullptr;
  }
}
