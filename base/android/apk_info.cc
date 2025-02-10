// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/apk_info.h"

#include <string>
#include <variant>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/ApkInfo_jni.h"

namespace base::android::apk_info {

namespace {

struct ApkInfo {
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
  bool targets_at_least_u;
  int target_sdk_version;
};

std::optional<ApkInfo> holder;

ApkInfo& get_apk_info() {
  [[maybe_unused]] static auto once = [] {
    Java_ApkInfo_nativeReadyForFields(AttachCurrentThread());
    return std::monostate();
  }();
  // holder should be initialized as the java is supposed to call the native
  // method FillFields which will initialize the fields within the holder.
  DCHECK(holder.has_value());
  return *holder;
}

}  // namespace

static void JNI_ApkInfo_FillFields(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& hostPackageName,
    const jni_zero::JavaParamRef<jstring>& hostVersionCode,
    const jni_zero::JavaParamRef<jstring>& hostPackageLabel,
    const jni_zero::JavaParamRef<jstring>& packageVersionCode,
    const jni_zero::JavaParamRef<jstring>& packageVersionName,
    const jni_zero::JavaParamRef<jstring>& packageName,
    const jni_zero::JavaParamRef<jstring>& resourcesVersion,
    const jni_zero::JavaParamRef<jstring>& installerPackageName,
    jboolean isDebugApp,
    jboolean targetsAtleastU,
    jint targetSdkVersion) {
  DCHECK(!holder.has_value());
  auto java_string_to_const_char =
      [](const jni_zero::JavaParamRef<jstring>& str) {
        return strdup(ConvertJavaStringToUTF8(str).c_str());
      };
  holder = ApkInfo{
      .host_package_name = java_string_to_const_char(hostPackageName),
      .host_version_code = java_string_to_const_char(hostVersionCode),
      .host_package_label = java_string_to_const_char(hostPackageLabel),
      .package_version_code = java_string_to_const_char(packageVersionCode),
      .package_version_name = java_string_to_const_char(packageVersionName),
      .package_name = java_string_to_const_char(packageName),
      .resources_version = java_string_to_const_char(resourcesVersion),
      .installer_package_name = java_string_to_const_char(installerPackageName),
      .is_debug_app = static_cast<bool>(isDebugApp),
      .targets_at_least_u = static_cast<bool>(targetsAtleastU),
      .target_sdk_version = targetSdkVersion};
}

const char* host_package_name() {
  return get_apk_info().host_package_name;
}

const char* host_version_code() {
  return get_apk_info().host_version_code;
}

const char* host_package_label() {
  return get_apk_info().host_package_label;
}

const char* package_version_code() {
  return get_apk_info().package_version_code;
}

const char* package_version_name() {
  return get_apk_info().package_version_name;
}

const char* package_name() {
  return get_apk_info().package_name;
}

const char* resources_version() {
  return get_apk_info().resources_version;
}

const char* installer_package_name() {
  return get_apk_info().installer_package_name;
}

bool is_debug_app() {
  return get_apk_info().is_debug_app;
}

int target_sdk_version() {
  return get_apk_info().target_sdk_version;
}

bool targets_at_least_u() {
  return get_apk_info().targets_at_least_u;
}
}  // namespace base::android::apk_info
