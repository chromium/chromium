// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TYPE_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TYPE_H_

namespace login_detection {

// Enumerates the different types of user log-in that can be detected on a
// page based on the site (effective TLD+1). This is recorded in metrics and
// should not be reordered or removed. Should be in sync with the same name in
// enums.xml
enum class LoginDetectionType {
  // No login was detected.
  kNoLogin,

  // OAuth login was detected for this site, and was remembered in persistent
  // memory.
  kDeprecatedOauthLogin,

  // Successful OAuth login flow was detected.
  kOauthFirstTimeLoginFlow,

  // The user had typed password to log-in. This includes sites where user
  // typed password manually or used Chrome password manager to fill-in.
  kDeprecatedPasswordEnteredLogin,

  // The site is in one of preloaded top sites where users commonly log-in.
  kDeprecatedPreloadedPasswordSiteLogin,

  // Treated as logged-in since as the site was retrieved from field trial as
  // commonly logged-in.
  kDeprecatedFieldTrialLoggedInSite,

  // The site has credentials saved in the password manager.
  kDeprecatedPasswordManagerSavedSite,

  // Successful popup based OAuth login flow was detected.
  kOauthPopUpFirstTimeLoginFlow,

  // Treated as logged-in since the site was detected as commonly logged-in from
  // optimization guide hints.
  kDeprecatedOptimizationGuideDetected,

  kMaxValue = kDeprecatedOptimizationGuideDetected
};
}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TYPE_H_
