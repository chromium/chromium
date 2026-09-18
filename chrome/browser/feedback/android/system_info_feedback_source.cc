// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/byte_size.h"
#include "base/system/sys_info.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/feedback/android/jni_headers/SystemInfoFeedbackSource_jni.h"

namespace chrome {
namespace android {

static std::string JNI_SystemInfoFeedbackSource_GetCpuArchitecture() {
  return base::SysInfo::OperatingSystemArchitecture();
}

static std::string JNI_SystemInfoFeedbackSource_GetGpuVendor() {
  gpu::GPUInfo info = content::GpuDataManager::GetInstance()->GetGPUInfo();

  return info.active_gpu().vendor_string;
}

static std::string JNI_SystemInfoFeedbackSource_GetGpuModel() {
  gpu::GPUInfo info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  return info.active_gpu().device_string;
}

static int JNI_SystemInfoFeedbackSource_GetAvailableMemoryMB() {
  return base::saturated_cast<int>(
      base::SysInfo::AmountOfAvailablePhysicalMemory().InMiB());
}

static int JNI_SystemInfoFeedbackSource_GetTotalMemoryMB() {
  return base::SysInfo::AmountOfTotalPhysicalMemory().InMiB();
}

}  // namespace android
}  // namespace chrome

DEFINE_JNI(SystemInfoFeedbackSource)
