// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"

#include "base/check.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"

namespace {
const char kTrialName[] = "SkiaBackend";
const char kGL[] = "GL";
const char kVulkan[] = "Vulkan";
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
  if (!manager->IsEssentialGpuInfoAvailable())
    return nullptr;
  if (manager->GetFeatureStatus(gpu::GpuFeatureType::GPU_FEATURE_TYPE_VULKAN) ==
      gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    return kVulkan;
  }
  return kGL;
}
