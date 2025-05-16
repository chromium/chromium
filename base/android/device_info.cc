// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/DeviceInfo_jni.h"
#include "base/synchronization/lock.h"

namespace base::android::device_info {
namespace {
struct DeviceInfo {
  std::string gms_version_code;
  bool is_tv;
  bool is_automotive;
  bool is_foldable;
  bool is_desktop;
  // Available only on Android T+.
  int32_t vulkan_deqp_level;
};

static std::optional<DeviceInfo>& get_holder() {
  static base::NoDestructor<std::optional<DeviceInfo>> holder;
  return *holder;
}

DeviceInfo& get_device_info() {
  std::optional<DeviceInfo>& holder = get_holder();
  if (!holder.has_value()) {
    Java_DeviceInfo_nativeReadyForFields(AttachCurrentThread());
  }
  return *holder;
}

}  // namespace

static void JNI_DeviceInfo_FillFields(JNIEnv* env,
                                      std::string& gmsVersionCode,
                                      jboolean isTV,
                                      jboolean isAutomotive,
                                      jboolean isFoldable,
                                      jboolean isDesktop,
                                      jint vulkanDeqpLevel) {
  std::optional<DeviceInfo>& holder = get_holder();
  DCHECK(!holder.has_value());
  holder.emplace(DeviceInfo{.gms_version_code = gmsVersionCode,
                            .is_tv = static_cast<bool>(isTV),
                            .is_automotive = static_cast<bool>(isAutomotive),
                            .is_foldable = static_cast<bool>(isFoldable),
                            .is_desktop = static_cast<bool>(isDesktop),
                            .vulkan_deqp_level = vulkanDeqpLevel});
}

const std::string& gms_version_code() {
  return get_device_info().gms_version_code;
}

void set_gms_version_code_for_test(const std::string& gms_version_code) {
  get_device_info().gms_version_code = gms_version_code;
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

}  // namespace base::android::device_info
