// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"

#include <string>

#include "base/android/android_info.h"
#include "base/android/apk_info.h"
#include "base/android/device_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/build_info_jni/BuildInfo_jni.h"

namespace base {
namespace android {

struct BuildInfoSingletonTraits {
  static BuildInfo* New() { return new BuildInfo(); }

  static void Delete(BuildInfo* x) {
    // We're leaking this type, see kRegisterAtExit.
    NOTREACHED();
  }

  static const bool kRegisterAtExit = false;
#if DCHECK_IS_ON()
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif
};

BuildInfo::BuildInfo()
    : brand_(android_info::brand()),
      device_(android_info::device()),
      android_build_id_(android_info::android_build_id()),
      manufacturer_(android_info::manufacturer()),
      model_(android_info::model()),
      sdk_int_(android_info::sdk_int()),
      build_type_(android_info::build_type()),
      board_(android_info::board()),
      host_package_name_(apk_info::host_package_name()),
      host_version_code_(apk_info::host_version_code()),
      host_package_label_(apk_info::host_package_label()),
      package_name_(apk_info::package_name()),
      package_version_code_(apk_info::package_version_code()),
      package_version_name_(apk_info::package_version_name()),
      android_build_fp_(android_info::android_build_fp()),
      installer_package_name_(apk_info::installer_package_name()),
      abi_name_(android_info::abi_name()),
      resources_version_(apk_info::resources_version()),
      target_sdk_version_(apk_info::target_sdk_version()),
      is_debug_android_(android_info::is_debug_android()),
      is_tv_(device_info::is_tv()),
      version_incremental_(android_info::version_incremental()),
      hardware_(android_info::hardware()),
      is_at_least_t_(android_info::is_at_least_t()),
      is_automotive_(device_info::is_automotive()),
      is_at_least_u_(android_info::is_at_least_u()),
      targets_at_least_u_(apk_info::targets_at_least_u()),
      codename_(android_info::codename()),
      vulkan_deqp_level_(device_info::vulkan_deqp_level()),
      is_foldable_(device_info::is_foldable()),
      soc_manufacturer_(android_info::soc_manufacturer()),
      is_debug_app_(apk_info::is_debug_app()),
      is_desktop_(device_info::is_desktop()) {}

BuildInfo::~BuildInfo() = default;

const char* BuildInfo::gms_version_code() const {
  return device_info::gms_version_code();
}

void BuildInfo::set_gms_version_code_for_test(
    const std::string& gms_version_code) {
  device_info::set_gms_version_code_for_test(gms_version_code);
}

std::string BuildInfo::host_signing_cert_sha256() {
  JNIEnv* env = AttachCurrentThread();
  return Java_BuildInfo_lazyGetHostSigningCertSha256(env);
}

// static
BuildInfo* BuildInfo::GetInstance() {
  return Singleton<BuildInfo, BuildInfoSingletonTraits>::get();
}

}  // namespace android
}  // namespace base
