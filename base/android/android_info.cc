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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android_info_jni/AndroidInfo_jni.h"

namespace base::android::android_info {

namespace {

static std::optional<AndroidInfo>& get_holder() {
  static base::NoDestructor<std::optional<AndroidInfo>> holder;
  return *holder;
}

const AndroidInfo& get_android_info() {
  const std::optional<AndroidInfo>& holder = get_holder();
  if (!holder.has_value()) {
    Java_AndroidInfo_nativeReadyForFields(AttachCurrentThread());
  }
  return *holder;
}

}  // namespace

AndroidInfo::AndroidInfo(const std::string& device,
                         const std::string& manufacturer,
                         const std::string& model,
                         const std::string& brand,
                         const std::string& android_build_id,
                         const std::string& build_type,
                         const std::string& board,
                         const std::string& android_build_fp,
                         int sdk_int,
                         bool is_debug_android,
                         const std::string& version_incremental,
                         const std::string& hardware,
                         const std::string& codename,
                         const std::string& soc_manufacturer,
                         const std::string& abi_name,
                         const std::string& security_patch)
    : device(device),
      manufacturer(manufacturer),
      model(model),
      brand(brand),
      android_build_id(android_build_id),
      build_type(build_type),
      board(board),
      android_build_fp(android_build_fp),
      sdk_int(sdk_int),
      is_debug_android(is_debug_android),
      version_incremental(version_incremental),
      hardware(hardware),
      codename(codename),
      soc_manufacturer(soc_manufacturer),
      abi_name(abi_name),
      security_patch(security_patch) {}

AndroidInfo::AndroidInfo(const AndroidInfo& android_info) = default;
AndroidInfo::~AndroidInfo() = default;

void SetAndroidInfoForTesting(const AndroidInfo& android_info) {
  std::optional<AndroidInfo>& holder = get_holder();
  holder.emplace(android_info);
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
                                       std::string& codeName,
                                       std::string& socManufacturer,
                                       std::string& supportedAbis,
                                       jint sdkInt,
                                       jboolean isDebugAndroid,
                                       std::string& securityPatch) {
  std::optional<AndroidInfo>& holder = get_holder();
  DCHECK(!holder.has_value());
  holder.emplace(AndroidInfo(device, manufacturer, model, brand, buildId, type,
                             board, androidBuildFingerprint, sdkInt,
                             static_cast<bool>(isDebugAndroid),
                             versionIncremental, hardware, codeName,
                             socManufacturer, supportedAbis, securityPatch));
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
  return get_android_info().android_build_id;
}

const std::string& build_type() {
  return get_android_info().build_type;
}

const std::string& board() {
  return get_android_info().board;
}

const std::string& android_build_fp() {
  return get_android_info().android_build_fp;
}

int sdk_int() {
  return get_android_info().sdk_int;
}

bool is_debug_android() {
  return get_android_info().is_debug_android;
}

const std::string& version_incremental() {
  return get_android_info().version_incremental;
}

const std::string& hardware() {
  return get_android_info().hardware;
}

const std::string& codename() {
  return get_android_info().codename;
}

// Available only on android S+. For S-, this method returns empty string.
const std::string& soc_manufacturer() {
  return get_android_info().soc_manufacturer;
}

const std::string& abi_name() {
  return get_android_info().abi_name;
}

const std::string& security_patch() {
  return get_android_info().security_patch;
}

}  // namespace base::android::android_info
