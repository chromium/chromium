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

#if __ANDROID_API__ >= 29
// .aidl based NDK generation is only available when our min SDK level is 29 or
// higher.
#include "aidl/org/chromium/base/IDeviceInfo.h"
using aidl::org::chromium::base::IDeviceInfo;
#endif

namespace base::android::device_info {
namespace {
#if __ANDROID_API__ < 29
struct IDeviceInfo {
  std::string gmsVersionCode;
  bool isAutomotive;
  bool isDesktop;
  bool isFoldable;
  bool isTv;
  // Available only on Android T+.
  int32_t vulkanDeqpLevel;
};
#endif

static std::optional<IDeviceInfo>& get_holder() {
  static base::NoDestructor<std::optional<IDeviceInfo>> holder;
  return *holder;
}

IDeviceInfo& get_device_info() {
  std::optional<IDeviceInfo>& holder = get_holder();
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
  std::optional<IDeviceInfo>& holder = get_holder();
  DCHECK(!holder.has_value());
  holder.emplace(IDeviceInfo{.gmsVersionCode = gmsVersionCode,
                             .isAutomotive = static_cast<bool>(isAutomotive),
                             .isDesktop = static_cast<bool>(isDesktop),
                             .isFoldable = static_cast<bool>(isFoldable),
                             .isTv = static_cast<bool>(isTV),
                             .vulkanDeqpLevel = vulkanDeqpLevel});
}

const std::string& gms_version_code() {
  return get_device_info().gmsVersionCode;
}

void set_gms_version_code_for_test(const std::string& gms_version_code) {
  get_device_info().gmsVersionCode = gms_version_code;
  Java_DeviceInfo_setGmsVersionCodeForTest(AttachCurrentThread(),
                                           gms_version_code);
}

bool is_tv() {
  return get_device_info().isTv;
}
bool is_automotive() {
  return get_device_info().isAutomotive;
}
bool is_foldable() {
  return get_device_info().isFoldable;
}

bool is_desktop() {
  return get_device_info().isDesktop;
}

// Available only on Android T+.
int32_t vulkan_deqp_level() {
  return get_device_info().vulkanDeqpLevel;
}

}  // namespace base::android::device_info
