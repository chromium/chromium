// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONFIRMATION_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONFIRMATION_H_

namespace privacy_sandbox {

/**
 * Determines whether Privacy Sandbox Ads consent is required.
 *
 * This function checks a collection of conditions to determine if the permanent
 * variation country is in a region where obtaining consent for the Privacy
 * Sandbox is necessary.  It also considers feature flags and overrides that
 * might influence the consent requirement.
 *
 * Returns `true` if user consent is required for Privacy Sandbox features,
 * `false` otherwise.
 *
 */
bool IsConsentRequired();

/**
 * Determines whether a the Privay Sandbox Ads notice is required.
 *
 * This function evaluates several criteria related to the Privacy Sandbox
 * feature, the permanent variation country, and potential feature overrides to
 * decide if a notice is necessary.
 *
 * Returns `true` if a privacy notice should be displayed, `false` otherwise.
 *
 */
bool IsNoticeRequired();

/**
 * Determines whether the Privacy Sandbox Ads Restricted notice is required.
 *
 * This function evaluates several criteria related to the Privacy Sandbox
 * feature,  other Privacy Sandbox notice requirements, and potential feature
 * overrides to decide if a restricted notice is necessary.
 *
 * Returns `true` if a Privacy Sandbox restricted notice is enabled.
 *
 */
bool IsRestrictedNoticeRequired();

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONFIRMATION_H_
