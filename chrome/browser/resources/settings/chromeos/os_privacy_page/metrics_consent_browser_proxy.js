// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import { addSingletonGetter, sendWithPromise } from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview Helper browser proxy for getting metrics and changing metrics
 * consent data.
 */

/**
 * MetricsConsentState represents the current metrics state for the current
 * logged-in user. prefName is the pref that is controlling the current user's
 * metrics consent state and isConfigurable indicates whether the current user
 * may change the pref.
 *
 * The prefName is currently always constant and only the owner of the device
 * may change the consent.
 *
 * @typedef {{
 *     prefName: string,
 *     isConfigurable: boolean,
 * }}
 */
export let MetricsConsentState;

/** @interface */
export class MetricsConsentBrowserProxy {
  /**
   * Returns the metrics consent state to render.
   *
   * @return {!Promise<MetricsConsentState>}
   */
  getMetricsConsentState() {}

  /**
   * Returns the new metrics consent after the update.
   *
   * @param {boolean} consent Consent to change metrics consent to.
   * @return {!Promise<boolean>}
   */
  updateMetricsConsent(consent) {}
}

/** @implements {MetricsConsentBrowserProxy} */
export class MetricsConsentBrowserProxyImpl {
  /** @override */
  getMetricsConsentState() {
    return sendWithPromise('getMetricsConsentState');
  }

  /** @override */
  updateMetricsConsent(consent) {
    return sendWithPromise('updateMetricsConsent', {consent: consent});
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(MetricsConsentBrowserProxyImpl);
