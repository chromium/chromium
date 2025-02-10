// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/DeviceInfo_jni.h"
#include "base/synchronization/lock.h"

namespace base::android::device_info {
namespace {
struct DeviceInfo {
  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  const char* gms_version_code;
  const char* custom_themes;
  bool is_tv;
  bool is_automotive;
  bool is_foldable;
  bool is_desktop;
  // Available only on Android T+.
  int32_t vulkan_deqp_level;
};

std::optional<DeviceInfo> holder;

DeviceInfo& get_device_info() {
  [[maybe_unused]] static auto once = [] {
    Java_DeviceInfo_nativeReadyForFields(AttachCurrentThread());
    return std::monostate();
  }();
  // holder should be initialized as the java is supposed to call the native
  // method FillFields which will initialize the fields within the holder.
  DCHECK(holder.has_value());
  return *holder;
}

}  // namespace

static void JNI_DeviceInfo_FillFields(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& gmsVersionCode,
    const jni_zero::JavaParamRef<jstring>& customThemes,
    jboolean isTV,
    jboolean isAutomotive,
    jboolean isFoldable,
    jboolean isDesktop,
    jint vulkanDeqpLevel) {
  DCHECK(!holder.has_value());
  auto java_string_to_const_char =
      [](const jni_zero::JavaParamRef<jstring>& str) {
        return strdup(ConvertJavaStringToUTF8(str).c_str());
      };
  holder =
      DeviceInfo{.gms_version_code = java_string_to_const_char(gmsVersionCode),
                 .custom_themes = java_string_to_const_char(customThemes),
                 .is_tv = static_cast<bool>(isTV),
                 .is_automotive = static_cast<bool>(isAutomotive),
                 .is_foldable = static_cast<bool>(isFoldable),
                 .is_desktop = static_cast<bool>(isDesktop),
                 .vulkan_deqp_level = vulkanDeqpLevel};
}

const char* gms_version_code() {
  return get_device_info().gms_version_code;
}

void set_gms_version_code_for_test(const std::string& gms_version_code) {
  get_device_info().gms_version_code = strdup(gms_version_code.c_str());
  Java_DeviceInfo_setGmsVersionCodeForTest(AttachCurrentThread(),
                                           gms_version_code);
}

bool is_tv() {
  return get_device_info().is_tv;
}
bool is_automotive() {
  return get_device_info().is_automotive;
}
bool is_foldable() {
  return get_device_info().is_foldable;
}

bool is_desktop() {
  return get_device_info().is_desktop;
}

// Available only on Android T+.
int32_t vulkan_deqp_level() {
  return get_device_info().vulkan_deqp_level;
}

const char* custom_themes() {
  return get_device_info().custom_themes;
}
}  // namespace base::android::device_info
