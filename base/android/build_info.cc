// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/BuildInfo_jni.h"

namespace base {
namespace android {

namespace {

// We are leaking these strings.
const char* StrDupParam(const std::vector<std::string>& params, size_t index) {
  return strdup(params[index].c_str());
}

int GetIntParam(const std::vector<std::string>& params, size_t index) {
  int ret = 0;
  bool success = StringToInt(params[index], &ret);
  DCHECK(success);
  return ret;
}

}  // namespace

struct BuildInfoSingletonTraits {
  static BuildInfo* New() {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> params_objs = Java_BuildInfo_getAll(env);
    std::vector<std::string> params;
    AppendJavaStringArrayToStringVector(env, params_objs, &params);
    return new BuildInfo(params);
  }

  static void Delete(BuildInfo* x) {
    // We're leaking this type, see kRegisterAtExit.
    NOTREACHED();
  }

  static const bool kRegisterAtExit = false;
#if DCHECK_IS_ON()
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif
};

BuildInfo::BuildInfo(const std::vector<std::string>& params)
    : brand_(StrDupParam(params, 0)),
      device_(StrDupParam(params, 1)),
      android_build_id_(StrDupParam(params, 2)),
      manufacturer_(StrDupParam(params, 3)),
      model_(StrDupParam(params, 4)),
      sdk_int_(GetIntParam(params, 5)),
      build_type_(StrDupParam(params, 6)),
      board_(StrDupParam(params, 7)),
      host_package_name_(StrDupParam(params, 8)),
      host_version_code_(StrDupParam(params, 9)),
      host_package_label_(StrDupParam(params, 10)),
      package_name_(StrDupParam(params, 11)),
      package_version_code_(StrDupParam(params, 12)),
      package_version_name_(StrDupParam(params, 13)),
      android_build_fp_(StrDupParam(params, 14)),
      gms_version_code_(StrDupParam(params, 15)),
      installer_package_name_(StrDupParam(params, 16)),
      abi_name_(StrDupParam(params, 17)),
      custom_themes_(StrDupParam(params, 18)),
      resources_version_(StrDupParam(params, 19)),
      target_sdk_version_(GetIntParam(params, 20)),
      is_debug_android_(GetIntParam(params, 21)),
      is_tv_(GetIntParam(params, 22)),
      version_incremental_(StrDupParam(params, 23)),
      hardware_(StrDupParam(params, 24)),
      is_at_least_t_(GetIntParam(params, 25)),
      is_automotive_(GetIntParam(params, 26)),
      is_at_least_u_(GetIntParam(params, 27)),
      targets_at_least_u_(GetIntParam(params, 28)),
      codename_(StrDupParam(params, 29)),
      vulkan_deqp_level_(GetIntParam(params, 30)),
      is_foldable_(GetIntParam(params, 31)),
      soc_manufacturer_(StrDupParam(params, 32)),
      is_debug_app_(GetIntParam(params, 33)),
      is_desktop_(GetIntParam(params, 34)) {}

BuildInfo::~BuildInfo() = default;

void BuildInfo::set_gms_version_code_for_test(
    const std::string& gms_version_code) {
  // This leaks the string, just like production code.
  gms_version_code_ = strdup(gms_version_code.c_str());
  Java_BuildInfo_setGmsVersionCodeForTest(AttachCurrentThread(),
                                          gms_version_code);
}

std::string BuildInfo::host_signing_cert_sha256() {
  JNIEnv* env = AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_BuildInfo_lazyGetHostSigningCertSha256(env));
}

// static
BuildInfo* BuildInfo::GetInstance() {
  return Singleton<BuildInfo, BuildInfoSingletonTraits >::get();
}

}  // namespace android
}  // namespace base
