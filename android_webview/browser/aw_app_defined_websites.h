// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
#define ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_

#include <string>
#include <vector>

namespace android_webview {

// Used to determine which sources to retrieve related
// websites from.
// This enum is used to set the values for feature parameter
// `features::kWebViewIpProtectionExclusionCriteria`.
// Do not modify/reorder the enum without ensuring that the above mentioned
// feature is compatible with the change.
enum class AppDefinedDomainCriteria {
  // Return nothing.
  kNone = 0,
  // Return domains defined in the `asset_statements` meta-data tag in the
  // app's manifest.
  kAndroidAssetStatements = 1,
  // For API >= 31, return domains defined in Android App Links and verified
  // by DomainVerificationManager.
  // For API < 31, return nothing.
  kAndroidVerifiedAppLinks = 2,
  // For API >= 31, return domains defined in Web Links (including Android
  // App Links).
  // For API < 31, return nothing.
  kAndroidWebLinks = 3,
  // Union of kAndroidAssetStatements, kAndroidVerifiedAppLinks and
  // kAndroidVerifiedAppLinks.
  kAndroidAssetStatementsAndWebLinks = 4,
};

std::vector<std::string> GetAppDefinedDomains(AppDefinedDomainCriteria policy);

// Returns if the `etld_plus1` requested is declared in the app's manifest.
// This is compared to
// AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks above. This
// method lazy loads and caches this list in a thread safe way on the first call
// for the remainder of the app lifecycle.
bool IsAppDefined(std::string etld_plus1);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
