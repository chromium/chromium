// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"

#include "base/check.h"

namespace android_webview {

constexpr char kWebViewAppsPackageNamesAllowlistComponentId[] =
    "aemllinfpjdgcldgaelcgakpjmaekbai";

namespace {
// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: aemllinfpjdgcldgaelcgakpjmaekbai
const uint8_t kWebViewAppsPackageNamesAllowlistPublicKeySHA256[32] = {
    0x04, 0xcb, 0xb8, 0xd5, 0xf9, 0x36, 0x2b, 0x36, 0x04, 0xb2, 0x60,
    0xaf, 0x9c, 0x04, 0xa1, 0x08, 0xa3, 0xe9, 0xdc, 0x92, 0x46, 0xe7,
    0xae, 0xc8, 0x3e, 0x32, 0x6f, 0x74, 0x43, 0x02, 0xf3, 0x7e};

}  // namespace

void GetWebViewAppsPackageNamesAllowlistPublicKeyHash(
    std::vector<uint8_t>* hash) {
  DCHECK(hash);
  hash->assign(kWebViewAppsPackageNamesAllowlistPublicKeySHA256,
               kWebViewAppsPackageNamesAllowlistPublicKeySHA256 +
                   std::size(kWebViewAppsPackageNamesAllowlistPublicKeySHA256));
}

}  // namespace android_webview
