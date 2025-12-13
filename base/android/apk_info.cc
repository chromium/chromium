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
#include "base/synchronization/lock.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/ApkInfo_jni.h"

#if __ANDROID_API__ >= 29
// .aidl based NDK generation is only available when our min SDK level is 29 or
// higher.
#include "aidl/org/chromium/base/IApkInfo.h"
using aidl::org::chromium::base::IApkInfo;
#endif

namespace base::android::apk_info {

namespace {
#if __ANDROID_API__ < 29
struct IApkInfo {
  const std::string hostPackageLabel;
  const std::string hostPackageName;
  const std::string hostVersionCode;
  const std::string installerPackageName;
  bool isDebugApp;
  const std::string packageName;
  const std::string packageVersionCode;
  const std::string packageVersionName;
  const std::string resourcesVersion;
  int targetSdkVersion;
};
#endif

static std::optional<IApkInfo>& get_holder() {
  static base::NoDestructor<std::optional<IApkInfo>> holder;
  return *holder;
}

const IApkInfo& get_apk_info() {
  const std::optional<IApkInfo>& holder = get_holder();
  if (!holder.has_value()) {
    Java_ApkInfo_nativeReadyForFields(AttachCurrentThread());
  }
  return *holder;
}

}  // namespace

void Set(const IApkInfo& info) {
  static base::NoDestructor<base::Lock> lock;
  base::AutoLock l(*lock);

  std::optional<IApkInfo>& holder = get_holder();
  if (holder.has_value()) {
    return;
  }
  holder.emplace(info);
}

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
  Set(IApkInfo{.hostPackageLabel = hostPackageLabel,
               .hostPackageName = hostPackageName,
               .hostVersionCode = hostVersionCode,
               .installerPackageName = installerPackageName,
               .isDebugApp = static_cast<bool>(isDebugApp),
               .packageName = packageName,
               .packageVersionCode = packageVersionCode,
               .packageVersionName = packageVersionName,
               .resourcesVersion = resourcesVersion,
               .targetSdkVersion = targetSdkVersion});
}

const std::string& host_package_name() {
  return get_apk_info().hostPackageName;
}

const std::string& host_version_code() {
  return get_apk_info().hostVersionCode;
}

const std::string& host_package_label() {
  return get_apk_info().hostPackageLabel;
}

const std::string& package_version_code() {
  return get_apk_info().packageVersionCode;
}

const std::string& package_version_name() {
  return get_apk_info().packageVersionName;
}

const std::string& package_name() {
  return get_apk_info().packageName;
}

const std::string& resources_version() {
  return get_apk_info().resourcesVersion;
}

const std::string& installer_package_name() {
  return get_apk_info().installerPackageName;
}

bool is_debug_app() {
  return get_apk_info().isDebugApp;
}

int target_sdk_version() {
  return get_apk_info().targetSdkVersion;
}

std::string host_signing_cert_sha256() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ApkInfo_getHostSigningCertSha256(env);
}
}  // namespace base::android::apk_info

DEFINE_JNI(ApkInfo)
