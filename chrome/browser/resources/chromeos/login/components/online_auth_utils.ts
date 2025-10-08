// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These values are used in histograms. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SamlRedirectEvent)
export enum SamlRedirectEvent {
  START_WITH_SSO_PROFILE = 0,
  START_WITH_DOMAIN = 1,
  CHANGE_TO_DEFAULT_GOOGLE_SIGN_IN = 2,
  MAX = 3,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/chromeos/enums.xml:SamlRedirectEvent,
// //chrome/browser/ash/login/saml/saml_test_utils.h:SamlRedirectEvent)

export enum LoginOrUnlock {
  LOGIN = 'Login',
  UNLOCK = 'Unlock',
}

export function recordUmaHistogramForSamlRedirectEvent(
    flow: LoginOrUnlock, event: SamlRedirectEvent): void {
  const histogramName = `ChromeOS.SAML.${flow}.SamlRedirectUsage`;
  chrome.send(
      'metricsHandler:recordInHistogram',
      [histogramName, event, SamlRedirectEvent.MAX]);
}
