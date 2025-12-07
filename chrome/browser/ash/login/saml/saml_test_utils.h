// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_SAML_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_SAML_TEST_UTILS_H_

// LINT.IfChange(SamlRedirectEvent)
enum class SamlRedirectEvent {
  kStartWithSsoProfile = 0,
  kStartWithDomain = 1,
  kChangeToDefaultGoogleSignIn = 2,
  kMaxValue = kChangeToDefaultGoogleSignIn,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/chromeos/enums.xml:SamlRedirectEvent,
// //chrome/browser/resources/chromeos/login/components/online_auth_utils.ts:SamlRedirectEvent)

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_SAML_TEST_UTILS_H_
