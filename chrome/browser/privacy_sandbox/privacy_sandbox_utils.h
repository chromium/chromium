// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_UTILS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_UTILS_H_

#include <string>

namespace privacy_sandbox {

// Used to determine the theme of the embedded privacy policy page.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
enum class PrivacyPolicyColorScheme { kLightMode = 0, kDarkMode = 1 };

// Used to determine the domain type of the embedded privacy policy page.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
enum class PrivacyPolicyDomainType { kNonChina = 0, kChina = 1 };

// Returns the URL for the embedded Privacy Policy page, tailored to the
// user's location (e.g., China), browser color scheme (dark or light), and
// locale.
std::string GetEmbeddedPrivacyPolicyURL(PrivacyPolicyDomainType domain_type,
                                        PrivacyPolicyColorScheme color_scheme,
                                        const std::string& locale);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_UTILS_H_
