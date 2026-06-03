// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_ANDROID_INFO_H_
#define BASE_ANDROID_ANDROID_INFO_H_

#include <string>

#include "base/base_export.h"
#if __ANDROID_API__ >= 29
namespace aidl::org::chromium::base {
class IAndroidInfo;
}  // namespace aidl::org::chromium::base
using ::aidl::org::chromium::base::IAndroidInfo;
#else
struct IAndroidInfo;
#endif

namespace base::android::android_info {

// This enumeration maps to the values returned by AndroidInfo::sdk_int(),
// indicating the Android release associated with a given SDK version.
enum SdkVersion {
  SDK_VERSION_JELLY_BEAN = 16,
  SDK_VERSION_JELLY_BEAN_MR1 = 17,
  SDK_VERSION_JELLY_BEAN_MR2 = 18,
  SDK_VERSION_KITKAT = 19,
  SDK_VERSION_KITKAT_WEAR = 20,
  SDK_VERSION_LOLLIPOP = 21,
  SDK_VERSION_LOLLIPOP_MR1 = 22,
  SDK_VERSION_MARSHMALLOW = 23,
  SDK_VERSION_NOUGAT = 24,
  SDK_VERSION_NOUGAT_MR1 = 25,
  SDK_VERSION_OREO = 26,
  SDK_VERSION_O_MR1 = 27,
  SDK_VERSION_P = 28,
  SDK_VERSION_Q = 29,
  SDK_VERSION_R = 30,
  SDK_VERSION_S = 31,
  SDK_VERSION_Sv2 = 32,
  SDK_VERSION_T = 33,
  SDK_VERSION_U = 34,
  SDK_VERSION_V = 35,
  SDK_VERSION_BAKLAVA = 36,
};

// This enumeration maps to the values returned by AndroidInfo::sdk_int_full(),
// indicating the minor Android release associated with a given SDK version.
enum SdkVersionFull {
  SDK_VERSION_FULL_JELLY_BEAN = 1600000,
  SDK_VERSION_FULL_JELLY_BEAN_MR1 = 1700000,
  SDK_VERSION_FULL_JELLY_BEAN_MR2 = 1800000,
  SDK_VERSION_FULL_KITKAT = 1900000,
  SDK_VERSION_FULL_KITKAT_WEAR = 2000000,
  SDK_VERSION_FULL_LOLLIPOP = 2100000,
  SDK_VERSION_FULL_LOLLIPOP_MR1 = 2200000,
  SDK_VERSION_FULL_MARSHMALLOW = 2300000,
  SDK_VERSION_FULL_NOUGAT = 2400000,
  SDK_VERSION_FULL_NOUGAT_MR1 = 2500000,
  SDK_VERSION_FULL_OREO = 2600000,
  SDK_VERSION_FULL_O_MR1 = 2700000,
  SDK_VERSION_FULL_P = 2800000,
  SDK_VERSION_FULL_Q = 2900000,
  SDK_VERSION_FULL_R = 3000000,
  SDK_VERSION_FULL_S = 3100000,
  SDK_VERSION_FULL_Sv2 = 3200000,
  SDK_VERSION_FULL_T = 3300000,
  SDK_VERSION_FULL_U = 3400000,
  SDK_VERSION_FULL_V = 3500000,
  SDK_VERSION_FULL_BAKLAVA = 3600000,
  SDK_VERSION_FULL_BAKLAVA_1 = 3600001,
};

BASE_EXPORT const std::string& device();

BASE_EXPORT const std::string& manufacturer();

BASE_EXPORT const std::string& model();

BASE_EXPORT const std::string& brand();

BASE_EXPORT const std::string& android_build_id();

BASE_EXPORT const std::string& build_type();

BASE_EXPORT const std::string& board();

BASE_EXPORT const std::string& android_build_fp();

BASE_EXPORT int sdk_int();

BASE_EXPORT int sdk_int_full();

BASE_EXPORT bool is_debug_android();

BASE_EXPORT const std::string& version_incremental();

BASE_EXPORT const std::string& hardware();

BASE_EXPORT const std::string& codename();

// Available only on android S+. For S-, this method returns empty string.
BASE_EXPORT const std::string& soc_manufacturer();

BASE_EXPORT const std::string& abi_name();

BASE_EXPORT const std::string& security_patch();

BASE_EXPORT void Set(const IAndroidInfo& info);
}  // namespace base::android::android_info

#endif  // BASE_ANDROID_ANDROID_INFO_H_
