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
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android_info_jni/AndroidInfo_jni.h"

namespace base::android::android_info {

namespace {

struct AndroidInfo {
  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  const char* device;

  const char* manufacturer;

  const char* model;

  const char* brand;

  const char* android_build_id;

  const char* build_type;

  const char* board;

  const char* android_build_fp;

  int sdk_int;

  bool is_debug_android;

  const char* version_incremental;

  const char* hardware;

  bool is_at_least_u;

  const char* codename;

  // Available only on android S+. For S-, this method returns empty string.
  const char* soc_manufacturer;

  bool is_at_least_t;

  const char* abi_name;
};

std::optional<AndroidInfo> holder;

const AndroidInfo& get_android_info() {
  [[maybe_unused]] static auto once = [] {
    Java_AndroidInfo_nativeReadyForFields(AttachCurrentThread());
    return std::monostate();
  }();
  // holder should be initialized as the java is supposed to call the native
  // method FillFields which will initialize the fields within the holder.
  DCHECK(holder.has_value());
  return *holder;
}

}  // namespace

static void JNI_AndroidInfo_FillFields(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& brand,
    const jni_zero::JavaParamRef<jstring>& device,
    const jni_zero::JavaParamRef<jstring>& buildId,
    const jni_zero::JavaParamRef<jstring>& manufacturer,
    const jni_zero::JavaParamRef<jstring>& model,
    const jni_zero::JavaParamRef<jstring>& type,
    const jni_zero::JavaParamRef<jstring>& board,
    const jni_zero::JavaParamRef<jstring>& androidBuildFingerprint,
    const jni_zero::JavaParamRef<jstring>& versionIncremental,
    const jni_zero::JavaParamRef<jstring>& hardware,
    const jni_zero::JavaParamRef<jstring>& codeName,
    const jni_zero::JavaParamRef<jstring>& socManufacturer,
    const jni_zero::JavaParamRef<jstring>& supportedAbis,
    jint sdkInt,
    jboolean isDebugAndroid,
    jboolean isAtleastU,
    jboolean isAtleastT) {
  DCHECK(!holder.has_value());
  auto java_string_to_const_char =
      [](const jni_zero::JavaParamRef<jstring>& str) {
        return strdup(ConvertJavaStringToUTF8(str).c_str());
      };
  holder = AndroidInfo{
      .device = java_string_to_const_char(device),
      .manufacturer = java_string_to_const_char(manufacturer),
      .model = java_string_to_const_char(model),
      .brand = java_string_to_const_char(brand),
      .android_build_id = java_string_to_const_char(buildId),
      .build_type = java_string_to_const_char(type),
      .board = java_string_to_const_char(board),
      .android_build_fp = java_string_to_const_char(androidBuildFingerprint),
      .sdk_int = sdkInt,
      .is_debug_android = static_cast<bool>(isDebugAndroid),
      .version_incremental = java_string_to_const_char(versionIncremental),
      .hardware = java_string_to_const_char(hardware),
      .is_at_least_u = static_cast<bool>(isAtleastU),
      .codename = java_string_to_const_char(codeName),
      .soc_manufacturer = java_string_to_const_char(socManufacturer),
      .is_at_least_t = static_cast<bool>(isAtleastT),
      .abi_name = java_string_to_const_char(supportedAbis),
  };
}

const char* device() {
  return get_android_info().device;
}

const char* manufacturer() {
  return get_android_info().manufacturer;
}

const char* model() {
  return get_android_info().model;
}

const char* brand() {
  return get_android_info().brand;
}

const char* android_build_id() {
  return get_android_info().android_build_id;
}

const char* build_type() {
  return get_android_info().build_type;
}

const char* board() {
  return get_android_info().board;
}

const char* android_build_fp() {
  return get_android_info().android_build_fp;
}

int sdk_int() {
  return get_android_info().sdk_int;
}

bool is_debug_android() {
  return get_android_info().is_debug_android;
}

const char* version_incremental() {
  return get_android_info().version_incremental;
}

const char* hardware() {
  return get_android_info().hardware;
}

bool is_at_least_u() {
  return get_android_info().is_at_least_u;
}

const char* codename() {
  return get_android_info().codename;
}

// Available only on android S+. For S-, this method returns empty string.
const char* soc_manufacturer() {
  return get_android_info().soc_manufacturer;
}

bool is_at_least_t() {
  return get_android_info().is_at_least_t;
}

const char* abi_name() {
  return get_android_info().abi_name;
}

}  // namespace base::android::android_info
