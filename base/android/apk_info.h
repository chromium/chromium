// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_APK_INFO_H_
#define BASE_ANDROID_APK_INFO_H_

#include <string>

#include "base/base_export.h"

#if __ANDROID_API__ >= 29
namespace aidl::org::chromium::base {
class IApkInfo;
}  // namespace aidl::org::chromium::base
using ::aidl::org::chromium::base::IApkInfo;
#else
struct IApkInfo;
#endif

namespace base::android::apk_info {
// The package name of the host app which has loaded WebView, retrieved from
// the application context. In the context of the SDK Runtime, the package
// name of the app that owns this particular instance of the SDK Runtime will
// also be included. e.g.
// com.google.android.sdksandbox:com:com.example.myappwithads
BASE_EXPORT const std::string& host_package_name();

// The application name (e.g. "Chrome"). For WebView, this is name of the
// embedding app. In the context of the SDK Runtime, this is the name of the
// app that owns this particular instance of the SDK Runtime.
BASE_EXPORT const std::string& host_version_code();

// By default: same as versionCode. For WebView: versionCode of the embedding
// app. In the context of the SDK Runtime, this is the versionCode of the app
// that owns this particular instance of the SDK Runtime.
BASE_EXPORT const std::string& host_package_label();

BASE_EXPORT const std::string& package_version_code();

BASE_EXPORT const std::string& package_version_name();

BASE_EXPORT const std::string& package_name();

BASE_EXPORT const std::string& resources_version();

BASE_EXPORT const std::string& installer_package_name();

BASE_EXPORT bool is_debug_app();

BASE_EXPORT int target_sdk_version();

BASE_EXPORT std::string host_signing_cert_sha256();

BASE_EXPORT void Set(const IApkInfo& info);
}  // namespace base::android::apk_info
#endif  // BASE_ANDROID_APK_INFO_H_
