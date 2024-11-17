// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/system_properties.h>

#include "base/android/sys_utils.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info_internal.h"

namespace {

// Default version of Android to fall back to when actual version numbers
// cannot be acquired. Use the latest Android release with a higher bug fix
// version to avoid unnecessarily comparison errors with the latest release.
// This should be manually kept up to date on each Android release.
const int kDefaultAndroidMajorVersion = 12;
const int kDefaultAndroidMinorVersion = 0;
const int kDefaultAndroidBugfixVersion = 99;

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
    int num_read = sscanf(os_version_str, "%d.%d.%d", major_version,
                          minor_version, bugfix_version);

    if (num_read > 0) {
      // If we don't have a full set of version numbers, make the extras 0.
      if (num_read < 2)
        *minor_version = 0;
      if (num_read < 3)
        *bugfix_version = 0;
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

std::string HardwareManufacturerName() {
  char device_model_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.manufacturer", device_model_str);
  return std::string(device_model_str);
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

std::string SysInfo::GetAndroidHardwareEGL() {
  char os_hardware_egl_str[PROP_VALUE_MAX];
  __system_property_get("ro.hardware.egl", os_hardware_egl_str);
  return std::string(os_hardware_egl_str);
}

static base::LazyInstance<base::internal::LazySysInfoValue<
    bool,
    android::SysUtils::IsLowEndDeviceFromJni>>::Leaky g_lazy_low_end_device =
    LAZY_INSTANCE_INITIALIZER;

bool SysInfo::IsLowEndDeviceImpl() {
  // This code might be used in some environments
  // which might not have a Java environment.
  // Note that we need to call the Java version here.
  // There exists a complete native implementation in
  // sys_info.cc but calling that here would mean that
  // the Java code and the native code would call different
  // implementations which could give different results.
  // Also the Java code cannot depend on the native code
  // since it might not be loaded yet.
  if (!base::android::IsVMInitialized())
    return false;
  return g_lazy_low_end_device.Get().value();
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = HardwareManufacturerName();
  info.model = HardwareModelName();
  DCHECK(IsStringUTF8(info.manufacturer));
  DCHECK(IsStringUTF8(info.model));
  return info;
}

}  // namespace base
