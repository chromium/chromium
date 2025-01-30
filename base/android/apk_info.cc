// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/apk_info.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/ApkInfo_jni.h"

namespace base::android::apk_info {

namespace {

struct ApkInfoHolder {
  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  const char* host_package_name;
  const char* host_version_code;
  const char* host_package_label;
  const char* package_version_code;
  const char* package_version_name;
  const char* package_name;
  const char* resources_version;
  const char* installer_package_name;
  bool is_debug_app;
  int target_sdk_version;
  bool targets_at_least_u;
};

const ApkInfoHolder& get_holder() {
  static const ApkInfoHolder info = [] {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> str_objs =
        Java_ApkInfo_getStringApkInfo(env);
    std::vector<std::string> str_params;
    AppendJavaStringArrayToStringVector(env, str_objs, &str_params);
    ScopedJavaLocalRef<jintArray> int_objs = Java_ApkInfo_getIntApkInfo(env);
    std::vector<int> int_params;
    JavaIntArrayToIntVector(env, int_objs, &int_params);
    return ApkInfoHolder{
        .host_package_name = strdup(str_params[0].c_str()),
        .host_version_code = strdup(str_params[1].c_str()),
        .host_package_label = strdup(str_params[2].c_str()),
        .package_version_code = strdup(str_params[3].c_str()),
        .package_version_name = strdup(str_params[4].c_str()),
        .package_name = strdup(str_params[5].c_str()),
        .resources_version = strdup(str_params[6].c_str()),
        .installer_package_name = strdup(str_params[7].c_str()),
        .is_debug_app = static_cast<bool>(int_params[0]),
        .target_sdk_version = int_params[1],
        .targets_at_least_u = static_cast<bool>(int_params[2])};
  }();
  return info;
}

}  // namespace

const char* host_package_name() {
  return get_holder().host_package_name;
}

const char* host_version_code() {
  return get_holder().host_version_code;
}

const char* host_package_label() {
  return get_holder().host_package_label;
}

const char* package_version_code() {
  return get_holder().package_version_code;
}

const char* package_version_name() {
  return get_holder().package_version_name;
}

const char* package_name() {
  return get_holder().package_name;
}

const char* resources_version() {
  return get_holder().resources_version;
}

const char* installer_package_name() {
  return get_holder().installer_package_name;
}

bool is_debug_app() {
  return get_holder().is_debug_app;
}

int target_sdk_version() {
  return get_holder().target_sdk_version;
}

bool targets_at_least_u() {
  return get_holder().targets_at_least_u;
}
}  // namespace base::android::apk_info
