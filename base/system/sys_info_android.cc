// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/system_properties.h>

#include "base/android/android_info.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"

namespace {

// Default version of Android to fall back to when actual version numbers
// cannot be acquired. Use a super high number in this case, as we assume
// it's due to being a pre-release version.
const int kDefaultAndroidMajorVersion = 9999;
const int kDefaultAndroidMinorVersion = 0;
const int kDefaultAndroidBugfixVersion = 0;

// Get and parse out the OS version numbers from the system properties.
// Note if parse fails, the "default" version is returned as fallback.
void GetOsVersionStringAndNumbers(std::string* version_string,
                                  int32_t* major_version,
                                  int32_t* minor_version,
                                  int32_t* bugfix_version) {
  // Read the version number string out from the properties.
  char os_version_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.release", os_version_str);

  if (os_version_str[0]) {
    // Try to parse out the version numbers from the string.
    int num_read = UNSAFE_TODO(sscanf(os_version_str, "%d.%d.%d", major_version,
                                      minor_version, bugfix_version));

    if (num_read > 0) {
      // If we don't have a full set of version numbers, make the extras 0.
      if (num_read < 2) {
        *minor_version = 0;
      }
      if (num_read < 3) {
        *bugfix_version = 0;
      }
      *version_string = std::string(os_version_str);
      return;
    }
  }

  // For some reason, we couldn't parse the version number string.
  *major_version = kDefaultAndroidMajorVersion;
  *minor_version = kDefaultAndroidMinorVersion;
  *bugfix_version = kDefaultAndroidBugfixVersion;
  *version_string = ::base::StringPrintf("%d.%d.%d", *major_version,
                                         *minor_version, *bugfix_version);
}

// Reads an Android system property of arbitrary length.
// This is preferred over `__system_property_get` because the legacy API is
// limited to `PROP_VALUE_MAX` (92 bytes) and will fail to return the value
// (returning a warning message instead) if the property is longer.
std::string ReadArbitrarilyLongSystemProperty(const char* name) {
  // `__system_property_read_callback` was introduced in Android API level 26.
  // When available, use it because it allows reading properties of arbitrary
  // length without being truncated or limited by `PROP_VALUE_MAX`.
  if (__builtin_available(android 26, *)) {
    const prop_info* pi = __system_property_find(name);
    if (!pi) {
      return std::string();
    }
    std::string value;
    __system_property_read_callback(
        pi,
        [](void* cookie, const char* /*name*/, const char* value,
           uint32_t /*serial*/) {
          // This static_cast is safe because:
          // 1. The cookie is passed as `&value` where `value` is a
          // `std::string`
          //    local variable in `ReadArbitrarilyLongSystemProperty`.
          // 2. `__system_property_read_callback` executes the callback
          //    synchronously on the same thread before returning.
          // 3. Therefore, the `value` object is guaranteed to be alive on the
          //    stack during the callback execution.
          std::string* out = static_cast<std::string*>(cookie);
          *out = value;
        },
        &value);
    return value;
  }

  // Fallback for devices running pre-API 26 or targets compiled with a
  // minimum deployment target lower than Android 26.
  char value_str[PROP_VALUE_MAX] = "";
  __system_property_get(name, value_str);
  return std::string(value_str);
}

}  // anonymous namespace

namespace base {

std::string SysInfo::HardwareModelName() {
  char device_model_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.model", device_model_str);
  return std::string(device_model_str);
}

std::string SysInfo::SocManufacturer() {
  char soc_manufacturer_str[PROP_VALUE_MAX];
  __system_property_get("ro.soc.manufacturer", soc_manufacturer_str);
  return std::string(soc_manufacturer_str);
}

std::string SysInfo::OperatingSystemName() {
  return "Android";
}

std::string SysInfo::OperatingSystemVersion() {
  std::string version_string;
  int32_t major, minor, bugfix;
  GetOsVersionStringAndNumbers(&version_string, &major, &minor, &bugfix);
  return version_string;
}

void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  std::string version_string;
  GetOsVersionStringAndNumbers(&version_string, major_version, minor_version,
                               bugfix_version);
}

std::string SysInfo::GetAndroidBuildCodename() {
  char os_version_codename_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.codename", os_version_codename_str);
  return std::string(os_version_codename_str);
}

std::string SysInfo::GetAndroidBuildID() {
  char os_build_id_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.id", os_build_id_str);
  return std::string(os_build_id_str);
}

std::string SysInfo::GetAndroidHardware() {
  char os_hardware_str[PROP_VALUE_MAX];
  __system_property_get("ro.hardware", os_hardware_str);
  return std::string(os_hardware_str);
}

std::string SysInfo::GetAndroidHardwareEGL() {
  char os_hardware_egl_str[PROP_VALUE_MAX];
  __system_property_get("ro.hardware.egl", os_hardware_egl_str);
  return std::string(os_hardware_egl_str);
}

std::string SysInfo::GetAndroidHardwareClass() {
  char os_hardware_id_str[PROP_VALUE_MAX];
  __system_property_get("ro.boot.product.hardware.id", os_hardware_id_str);
  return std::string(os_hardware_id_str);
}

// static
std::string SysInfo::HardwareManufacturer() {
  char device_model_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.manufacturer", device_model_str);
  return std::string(device_model_str);
}

// static
bool SysInfo::HasLargeProcessCountSupport() {
  // Safesetid is only guaranteed to be enabled for 6.18 kernel on Android 17
  // and therefore the process limit can only be increased for devices with a
  // new enough kernel that are also running Android 17.
  //
  // TODO(crbug.com/422903297): Expand coverage as noted in the bug, as it's
  // stricter than it needs to be, on ARM64 in particular.
  static const bool has_support = [] {
    return android::android_info::sdk_int() >=
               android::android_info::SDK_VERSION_CINNAMON_BUN &&
           KernelVersionNumber::Current() >= KernelVersionNumber{6, 18, 0};
  }();
  return has_support;
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = HardwareManufacturer();
  info.model = HardwareModelName();
  DCHECK(IsStringUTF8(info.manufacturer));
  DCHECK(IsStringUTF8(info.model));
  return info;
}

std::string SysInfo::GetAndroidBuildFingerprint() {
  return ReadArbitrarilyLongSystemProperty("ro.build.fingerprint");
}

}  // namespace base
