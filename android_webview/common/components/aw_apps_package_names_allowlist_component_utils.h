// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_COMPONENTS_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_UTILS_H_
#define ANDROID_WEBVIEW_COMMON_COMPONENTS_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_UTILS_H_

#include <stdint.h>

#include <vector>

namespace android_webview {

extern const char kWebViewAppsPackageNamesAllowlistComponentId[];

void GetWebViewAppsPackageNamesAllowlistPublicKeyHash(
    std::vector<uint8_t>* hash);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_COMPONENTS_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_UTILS_H_
