// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';

/**
 * The GA library URL in trusted type.
 */
const gaLibraryURL = (() => {
  const staticUrlPolicy =
      assertExists(window.trustedTypes).createPolicy('ga-js-static', {
        createScriptURL: (_url: string) => '../js/lib/analytics.js',
      });
  return staticUrlPolicy.createScriptURL('');
})();

declare global {
  interface Window {
    // Used for GA to support renaming, see
    // https://developers.google.com/analytics/devguides/collection/analyticsjs/tracking-snippet-reference#tracking-unminified
    // eslint-disable-next-line @typescript-eslint/naming-convention
    GoogleAnalyticsObject: 'ga';
    // GA use global `ga-disable-GA_MEASUREMENT_ID` to disable a particular
    // measurement. See
    // https://developers.google.com/analytics/devguides/collection/gtagjs/user-opt-out
    [key: `ga-disable-${string}`]: boolean;
  }
}

/**
 * Initializes GA for sending metrics.
 *
 * @param id The GA tracker ID to send metrics.
 * @param clientId The GA client ID representing the current client.
 * @param setClientIdCallback Callback to store client id.
 */
function initGA(
    id: string, clientId: string,
    setClientIdCallback: (clientId: string) => void): void {
  // GA initialization function which is copied and inlined from
  // https://developers.google.com/analytics/devguides/collection/analyticsjs.
  window.GoogleAnalyticsObject = 'ga';
  // Creates an initial ga() function.
  // The queued commands will be executed once analytics.js loads.
  window.ga = window.ga || ((...args: unknown[]) => {
                             (window.ga.q = window.ga.q || []).push(args);
                           }) as UniversalAnalytics.ga;
  window.ga.l = Date.now();
  const a = document.createElement('script');
  const m = document.getElementsByTagName('script')[0];
  a.async = true;
  // TypeScript doesn't support setting .src to TrustedScriptURL yet.
  a.src = gaLibraryURL as unknown as string;
  assert(m.parentNode !== null);
  m.parentNode.insertBefore(a, m);

  window.ga('create', id, {
    storage: 'none',
    clientId: clientId,
  });

  window.ga((tracker?: UniversalAnalytics.Tracker) => {
    assert(tracker !== undefined);
    setClientIdCallback(tracker.get('clientId'));
  });

  // By default GA uses a fake image and sets its source to the target URL to
  // record metrics. Since requesting remote image violates the policy of
  // a platform app, use navigator.sendBeacon() instead.
  window.ga('set', 'transport', 'beacon');

  // By default GA only accepts "http://" and "https://" protocol. Bypass the
  // check here since we are "chrome-extension://".
  window.ga('set', 'checkProtocolTask', null);

  // GA automatically derives geographical data from the IP address. Truncate
  // the IP address to avoid violating privacy policy.
  window.ga('set', 'anonymizeIp', true);
}

/**
 * Sends event to GA.
 *
 * @param event Event to send.
 */
function sendGAEvent(event: UniversalAnalytics.FieldsObject): void {
  window.ga('send', 'event', event);
}

/**
 * Sets if GA can send metrics.
 *
 * @param id The GA tracker ID.
 * @param enabled True if the metrics is enabled.
 */
function setMetricsEnabled(id: string, enabled: boolean): void {
  window[`ga-disable-${id}`] = !enabled;
}

export interface GAHelper {
  initGA: typeof initGA;
  sendGAEvent: typeof sendGAEvent;
  setMetricsEnabled: typeof setMetricsEnabled;
}
export {initGA, sendGAEvent, setMetricsEnabled};
