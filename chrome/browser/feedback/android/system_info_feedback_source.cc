// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/system/sys_info.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feedback/android/jni_headers/SystemInfoFeedbackSource_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

std::string JNI_SystemInfoFeedbackSource_GetCpuArchitecture(JNIEnv* env) {
  return base::SysInfo::OperatingSystemArchitecture();
}

std::string JNI_SystemInfoFeedbackSource_GetGpuVendor(JNIEnv* env) {
  gpu::GPUInfo info = content::GpuDataManager::GetInstance()->GetGPUInfo();

  return info.active_gpu().vendor_string;
}

std::string JNI_SystemInfoFeedbackSource_GetGpuModel(JNIEnv* env) {
  gpu::GPUInfo info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  return info.active_gpu().device_string;
}

int JNI_SystemInfoFeedbackSource_GetAvailableMemoryMB(JNIEnv* env) {
  return base::saturated_cast<int>(
      base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024 / 1024);
}

int JNI_SystemInfoFeedbackSource_GetTotalMemoryMB(JNIEnv* env) {
  return base::SysInfo::AmountOfPhysicalMemoryMB();
}

}  // namespace android
}  // namespace chrome
