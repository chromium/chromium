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
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/ApkInfo_jni.h"

namespace base::android::apk_info {

namespace {

struct ApkInfo {
  const std::string host_package_name;
  const std::string host_version_code;
  const std::string host_package_label;
  const std::string package_version_code;
  const std::string package_version_name;
  const std::string package_name;
  const std::string resources_version;
  const std::string installer_package_name;
  bool is_debug_app;
  int target_sdk_version;
};

static std::optional<ApkInfo>& get_holder() {
  static base::NoDestructor<std::optional<ApkInfo>> holder;
  return *holder;
}

const ApkInfo& get_apk_info() {
  const std::optional<ApkInfo>& holder = get_holder();
  if (!holder.has_value()) {
    Java_ApkInfo_nativeReadyForFields(AttachCurrentThread());
  }
  return *holder;
}

}  // namespace

static void JNI_ApkInfo_FillFields(JNIEnv* env,
                                   std::string& hostPackageName,
                                   std::string& hostVersionCode,
                                   std::string& hostPackageLabel,
                                   std::string& packageVersionCode,
                                   std::string& packageVersionName,
                                   std::string& packageName,
                                   std::string& resourcesVersion,
                                   std::string& installerPackageName,
                                   jboolean isDebugApp,
                                   jint targetSdkVersion) {
  std::optional<ApkInfo>& holder = get_holder();
  DCHECK(!holder.has_value());
  holder.emplace(ApkInfo{.host_package_name = hostPackageName,
                         .host_version_code = hostVersionCode,
                         .host_package_label = hostPackageLabel,
                         .package_version_code = packageVersionCode,
                         .package_version_name = packageVersionName,
                         .package_name = packageName,
                         .resources_version = resourcesVersion,
                         .installer_package_name = installerPackageName,
                         .is_debug_app = static_cast<bool>(isDebugApp),
                         .target_sdk_version = targetSdkVersion});
}

const std::string& host_package_name() {
  return get_apk_info().host_package_name;
}

const std::string& host_version_code() {
  return get_apk_info().host_version_code;
}

const std::string& host_package_label() {
  return get_apk_info().host_package_label;
}

const std::string& package_version_code() {
  return get_apk_info().package_version_code;
}

const std::string& package_version_name() {
  return get_apk_info().package_version_name;
}

const std::string& package_name() {
  return get_apk_info().package_name;
}

const std::string& resources_version() {
  return get_apk_info().resources_version;
}

const std::string& installer_package_name() {
  return get_apk_info().installer_package_name;
}

bool is_debug_app() {
  return get_apk_info().is_debug_app;
}

int target_sdk_version() {
  return get_apk_info().target_sdk_version;
}

}  // namespace base::android::apk_info
