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
struct DeviceInfoHolder {
  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  const char* gms_version_code;
  bool is_tv;
  bool is_automotive;
  bool is_foldable;
  bool is_desktop;
  // Available only on Android T+.
  int32_t vulkan_deqp_level;
  const char* custom_themes;
};

DeviceInfoHolder& get_holder() {
  static DeviceInfoHolder info = [] {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> str_objs =
        Java_DeviceInfo_getStringDeviceInfo(env);
    std::vector<std::string> str_params;
    AppendJavaStringArrayToStringVector(env, str_objs, &str_params);
    ScopedJavaLocalRef<jintArray> int_objs =
        Java_DeviceInfo_getIntDeviceInfo(env);
    std::vector<int> int_params;
    JavaIntArrayToIntVector(env, int_objs, &int_params);
    return DeviceInfoHolder{.gms_version_code = strdup(str_params[0].c_str()),
                            .is_tv = static_cast<bool>(int_params[0]),
                            .is_automotive = static_cast<bool>(int_params[1]),
                            .is_foldable = static_cast<bool>(int_params[2]),
                            .is_desktop = static_cast<bool>(int_params[3]),
                            .vulkan_deqp_level = int_params[4],
                            .custom_themes = strdup(str_params[1].c_str())};
  }();
  return info;
}

}  // namespace

const char* gms_version_code() {
  return get_holder().gms_version_code;
}

void set_gms_version_code_for_test(const std::string& gms_version_code) {
  get_holder().gms_version_code = strdup(gms_version_code.c_str());
  Java_DeviceInfo_setGmsVersionCodeForTest(AttachCurrentThread(),
                                           gms_version_code);
}

bool is_tv() {
  return get_holder().is_tv;
}
bool is_automotive() {
  return get_holder().is_automotive;
}
bool is_foldable() {
  return get_holder().is_foldable;
}

bool is_desktop() {
  return get_holder().is_desktop;
}

// Available only on Android T+.
int32_t vulkan_deqp_level() {
  return get_holder().vulkan_deqp_level;
}

const char* custom_themes() {
  return get_holder().custom_themes;
}
}  // namespace base::android::device_info
