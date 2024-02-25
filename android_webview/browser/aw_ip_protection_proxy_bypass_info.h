// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_PROXY_BYPASS_INFO_H_
#define ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_PROXY_BYPASS_INFO_H_

#include <string>
#include <vector>

namespace android_webview {

// Exclusion policy for determining which domains get excluded from the
// Masked Domain List for WebView.
// This enum is used to set the values for feature parameter
// `features::kWebViewIpProtectionExclusionCriteria`.
// Do not modify/reorder the enum without ensuring that the above mentioned
// feature is compatible with the change.
enum class WebviewExclusionPolicy {
  // Exclude nothing.
  kNone = 0,
  // Exclude domains defined in the `asset_statements` meta-data tag in the
  // app's manifest.
  kAndroidAssetStatements = 1,
  // For API >= 31, exclude domains defined in Android App Links and verified
  // by DomainVerificationManager.
  // For API < 31, exclude nothing.
  kAndroidVerifiedAppLinks = 2,
  // For API >= 31, exclude domains defined in Web Links (including Android
  // App Links).
  // For API < 31, exclude nothing.
  kAndroidWebLinks = 3,
  // Union of kAndroidAssetStatements, kAndroidVerifiedAppLinks and
  // kAndroidVerifiedAppLinks.
  kAndroidAssetStatementsAndWebLinks = 4,
};

// Fetches domains that should be excluded from the masked domain list.
std::vector<std::string> LoadExclusionList();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_PROXY_BYPASS_INFO_H_
