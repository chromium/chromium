// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_DEVICE_INFO_H_
#define BASE_ANDROID_DEVICE_INFO_H_

#include <string>

#include "base/base_export.h"

#if __ANDROID_API__ >= 29
namespace aidl::org::chromium::base {
class IDeviceInfo;
}  // namespace aidl::org::chromium::base
using ::aidl::org::chromium::base::IDeviceInfo;
#else
struct IDeviceInfo;
#endif

namespace base::android::device_info {
BASE_EXPORT const std::string& gms_version_code();

BASE_EXPORT void set_gms_version_code_for_test(
    const std::string& gms_version_code);

BASE_EXPORT void Set(const IDeviceInfo& info);

BASE_EXPORT bool is_tv();
BASE_EXPORT bool is_automotive();
BASE_EXPORT bool is_foldable();
BASE_EXPORT bool is_desktop();
// Available only on Android T+.
BASE_EXPORT int32_t vulkan_deqp_level();
BASE_EXPORT bool is_xr();
BASE_EXPORT bool was_launched_on_large_display();  // >= 600dp
BASE_EXPORT bool is_tablet();
BASE_EXPORT std::string device_name();

// For testing use only.
BASE_EXPORT void set_is_xr_for_testing();
BASE_EXPORT void reset_is_xr_for_testing();
}  // namespace base::android::device_info

#endif  // BASE_ANDROID_DEVICE_INFO_H_
