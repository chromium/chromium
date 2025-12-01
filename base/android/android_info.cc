// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_info.h"

#include <cstring>
#include <mutex>
#include <string>
#include <variant>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/AndroidInfo_jni.h"

#if __ANDROID_API__ >= 29
// .aidl based NDK generation is only available when our min SDK level is 29 or
// higher.
#include "aidl/org/chromium/base/IAndroidInfo.h"
using aidl::org::chromium::base::IAndroidInfo;
#endif

#if __ANDROID_API__ < 29
struct IAndroidInfo {
  const std::string abiName;
  const std::string androidBuildFp;
  const std::string androidBuildId;
  const std::string board;
  const std::string brand;
  const std::string buildType;
  const std::string codename;
  const std::string device;
  const std::string hardware;
  bool isDebugAndroid;
  const std::string manufacturer;
  const std::string model;
  int sdkInt;
  const std::string securityPatch;
  // Available only on android S+. For S-, this method returns empty string.
  const std::string socManufacturer;
  const std::string versionIncremental;
};
#endif

namespace base::android::android_info {

namespace {

static std::optional<IAndroidInfo>& get_holder() {
  static base::NoDestructor<std::optional<IAndroidInfo>> holder;
  return *holder;
}

const IAndroidInfo& get_android_info() {
  const std::optional<IAndroidInfo>& holder = get_holder();
  if (!holder.has_value()) {
    Java_AndroidInfo_nativeReadyForFields(AttachCurrentThread());
  }
  return *holder;
}

}  // namespace

void Set(const IAndroidInfo& info) {
  static base::NoDestructor<base::Lock> lock;
  base::AutoLock l(*lock);

  std::optional<IAndroidInfo>& holder = get_holder();
  if (holder.has_value()) {
    return;
  }
  holder.emplace(info);
}

static void JNI_AndroidInfo_FillFields(JNIEnv* env,
                                       std::string& brand,
                                       std::string& device,
                                       std::string& buildId,
                                       std::string& manufacturer,
                                       std::string& model,
                                       std::string& type,
                                       std::string& board,
                                       std::string& androidBuildFingerprint,
                                       std::string& versionIncremental,
                                       std::string& hardware,
                                       std::string& codename,
                                       std::string& socManufacturer,
                                       std::string& supportedAbis,
                                       jint sdkInt,
                                       jboolean isDebugAndroid,
                                       std::string& securityPatch) {
  Set(IAndroidInfo{.abiName = supportedAbis,
                   .androidBuildFp = androidBuildFingerprint,
                   .androidBuildId = buildId,
                   .board = board,
                   .brand = brand,
                   .buildType = type,
                   .codename = codename,
                   .device = device,
                   .hardware = hardware,
                   .isDebugAndroid = static_cast<bool>(isDebugAndroid),
                   .manufacturer = manufacturer,
                   .model = model,
                   .sdkInt = sdkInt,
                   .securityPatch = securityPatch,
                   .socManufacturer = socManufacturer,
                   .versionIncremental = versionIncremental});
}

const std::string& device() {
  return get_android_info().device;
}

const std::string& manufacturer() {
  return get_android_info().manufacturer;
}

const std::string& model() {
  return get_android_info().model;
}

const std::string& brand() {
  return get_android_info().brand;
}

const std::string& android_build_id() {
  return get_android_info().androidBuildId;
}

const std::string& build_type() {
  return get_android_info().buildType;
}

const std::string& board() {
  return get_android_info().board;
}

const std::string& android_build_fp() {
  return get_android_info().androidBuildFp;
}

int sdk_int() {
  return get_android_info().sdkInt;
}

bool is_debug_android() {
  return get_android_info().isDebugAndroid;
}

const std::string& version_incremental() {
  return get_android_info().versionIncremental;
}

const std::string& hardware() {
  return get_android_info().hardware;
}

const std::string& codename() {
  return get_android_info().codename;
}

// Available only on android S+. For S-, this method returns empty string.
const std::string& soc_manufacturer() {
  return get_android_info().socManufacturer;
}

const std::string& abi_name() {
  return get_android_info().abiName;
}

const std::string& security_patch() {
  return get_android_info().securityPatch;
}

}  // namespace base::android::android_info

DEFINE_JNI(AndroidInfo)
