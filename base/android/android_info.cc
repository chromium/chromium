// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_info.h"

#include <cstring>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android_info_jni/AndroidInfo_jni.h"

namespace base::android::android_info {

namespace {

struct AndroidInfoHolder {
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

const AndroidInfoHolder& get_holder() {
  static const AndroidInfoHolder info = [] {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> str_objs =
        Java_AndroidInfo_getStringAndroidInfo(env);
    std::vector<std::string> str_params;
    AppendJavaStringArrayToStringVector(env, str_objs, &str_params);
    ScopedJavaLocalRef<jintArray> int_objs =
        Java_AndroidInfo_getIntAndroidInfo(env);
    std::vector<int> int_params;
    JavaIntArrayToIntVector(env, int_objs, &int_params);
    return AndroidInfoHolder{
        .device = strdup(str_params[0].c_str()),
        .manufacturer = strdup(str_params[1].c_str()),
        .model = strdup(str_params[2].c_str()),
        .brand = strdup(str_params[3].c_str()),
        .android_build_id = strdup(str_params[4].c_str()),
        .build_type = strdup(str_params[5].c_str()),
        .board = strdup(str_params[6].c_str()),
        .android_build_fp = strdup(str_params[7].c_str()),
        .sdk_int = int_params[0],
        .is_debug_android = static_cast<bool>(int_params[1]),
        .version_incremental = strdup(str_params[8].c_str()),
        .hardware = strdup(str_params[9].c_str()),
        .is_at_least_u = static_cast<bool>(int_params[2]),
        .codename = strdup(str_params[10].c_str()),
        .soc_manufacturer = strdup(str_params[11].c_str()),
        .is_at_least_t = static_cast<bool>(int_params[3]),
        .abi_name = strdup(str_params[12].c_str())};
  }();
  return info;
}

}  // namespace

const char* device() {
  return get_holder().device;
}

const char* manufacturer() {
  return get_holder().manufacturer;
}

const char* model() {
  return get_holder().model;
}

const char* brand() {
  return get_holder().brand;
}

const char* android_build_id() {
  return get_holder().android_build_id;
}

const char* build_type() {
  return get_holder().build_type;
}

const char* board() {
  return get_holder().board;
}

const char* android_build_fp() {
  return get_holder().android_build_fp;
}

int sdk_int() {
  return get_holder().sdk_int;
}

bool is_debug_android() {
  return get_holder().is_debug_android;
}

const char* version_incremental() {
  return get_holder().version_incremental;
}

const char* hardware() {
  return get_holder().hardware;
}

bool is_at_least_u() {
  return get_holder().is_at_least_u;
}

const char* codename() {
  return get_holder().codename;
}

// Available only on android S+. For S-, this method returns empty string.
const char* soc_manufacturer() {
  return get_holder().soc_manufacturer;
}

bool is_at_least_t() {
  return get_holder().is_at_least_t;
}

const char* abi_name() {
  return get_holder().abi_name;
}
}  // namespace base::android::android_info
