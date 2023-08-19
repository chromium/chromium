// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper browser proxy for getting metrics and changing metrics
 * consent data.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * MetricsConsentState represents the current metrics state for the current
 * logged-in user. prefName is the pref that is controlling the current user's
 * metrics consent state and isConfigurable indicates whether the current user
 * may change the pref.
 *
 * The prefName is currently always constant and only the owner of the device
 * may change the consent.
 */
export interface MetricsConsentState {
  prefName: string;
  isConfigurable: boolean;
}

export interface MetricsConsentBrowserProxy {
  /**
   * Returns the metrics consent state to render.
   */
  getMetricsConsentState(): Promise<MetricsConsentState>;

  /**
   * Returns the new metrics consent after the update.
   * @param consent Consent to change metrics consent to.
   */
  updateMetricsConsent(consent: boolean): Promise<boolean>;
}

let instance: MetricsConsentBrowserProxy|null = null;

export class MetricsConsentBrowserProxyImpl implements
    MetricsConsentBrowserProxy {
  static getInstance(): MetricsConsentBrowserProxy {
    return instance || (instance = new MetricsConsentBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: MetricsConsentBrowserProxy): void {
    instance = obj;
  }

  getMetricsConsentState(): Promise<MetricsConsentState> {
    return sendWithPromise('getMetricsConsentState');
  }

  updateMetricsConsent(consent: boolean): Promise<boolean> {
    return sendWithPromise('updateMetricsConsent', {consent});
  }
}
