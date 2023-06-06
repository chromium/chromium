// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import {WaitableEvent} from './waitable_event.js';

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

type SendGA4Event = typeof sendGA4Event;
const sendGA4EventReady = new WaitableEvent<SendGA4Event>();
interface InitGAIdParams {
  gaId: string;
  ga4Id: string;
  clientId: string;
  ga4ApiSecret: string;
  ga4SessionId: string;
}

/**
 * Initializes GA and GA4 for sending metrics.
 *
 * @param idParams The parameters to initialize GA and GA4.
 * @param idParams.gaId The GA tracker ID to send events.
 * @param idParams.ga4Id The GA4 measurement ID to send events.
 * @param idParams.clientId The client ID for the current client for GA and GA4.
 * @param setClientIdCallback Callback to store client id.
 */
function initGA(
    idParams: InitGAIdParams,
    setClientIdCallback: (clientId: string) => void): void {
  // GA initialization function which is copied and inlined from
  // https://developers.google.com/analytics/devguides/collection/analyticsjs.
  window.GoogleAnalyticsObject = 'ga';
  // Creates an initial ga() function.
  // The queued commands will be executed once analytics.js loads.
  //
  // The type of .ga on Window doesn't include undefined, but since this part
  // is setup code for ga, it's possible to have a undefined case here. Disable
  // eslint which would think the condition is always true.
  //
  // The type assertion is also needed since this part of invariant is
  // maintained by ga itself, and all our usage only use the function call
  // interface.
  /* eslint-disable
       @typescript-eslint/strict-boolean-expressions,
       @typescript-eslint/consistent-type-assertions */
  window.ga = window.ga || ((...args: unknown[]) => {
                             (window.ga.q = window.ga.q || []).push(args);
                           }) as UniversalAnalytics.ga;
  /* eslint-enable
       @typescript-eslint/strict-boolean-expressions,
       @typescript-eslint/consistent-type-assertions */
  window.ga.l = Date.now();
  const a = document.createElement('script');
  const m = document.getElementsByTagName('script')[0];
  a.async = true;
  // TypeScript doesn't support setting .src to TrustedScriptURL yet.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  a.src = gaLibraryURL as unknown as string;
  assert(m.parentNode !== null);
  m.parentNode.insertBefore(a, m);

  const {gaId, ga4Id, clientId, ga4ApiSecret, ga4SessionId} = idParams;
  window.ga('create', gaId, {
    storage: 'none',
    clientId: clientId,
  });

  window.ga((tracker?: UniversalAnalytics.Tracker) => {
    assert(tracker !== undefined);
    const clientId = tracker.get('clientId');
    setClientIdCallback(clientId);
    sendGA4EventReady.signal(genSendGA4Event({
      ga4Id,
      gaId,
      clientId,
      ga4ApiSecret,
      ga4SessionId,
    }));
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

function genSendGA4Event({gaId, ga4Id, clientId, ga4ApiSecret, ga4SessionId}:
                             InitGAIdParams): SendGA4Event {
  return (event: UniversalAnalytics.FieldsObject,
          dimensions: Record<string, string>) => {
    if (window[`ga-disable-${gaId}`]) {
      return;
    }
    // TODO(b/267265966): Use gtag.js instead of measurement protocol when
    // gtag.js supports sending events under non-http/https protocols. GA4 uses
    // `engagement_time_msec` and `session_id` to calculate user activity.
    // Remove these parameters as they are sent automatically by gtag.js.
    /* eslint-disable @typescript-eslint/naming-convention */
    const params: Record<string, unknown> = {
      ...dimensions,
      // Set '1' here as it's enough for GA4 to generate the metrics for n-day
      // active users and we don't want to reimplement how gtag.js calculate the
      // engagement time for each event.
      engagement_time_msec: '1',
      session_id: ga4SessionId,
    };
    if (event.eventLabel !== undefined) {
      params['event_label'] = event.eventLabel;
    }
    if (event.eventCategory !== undefined) {
      params['event_category'] = event.eventCategory;
    }
    if (event.eventValue !== undefined) {
      params['value'] = event.eventValue;
    }
    void fetch(
        `https://www.google-analytics.com/mp/collect?measurement_id=${
            ga4Id}&api_secret=${ga4ApiSecret}`,
        {
          method: 'POST',
          body: JSON.stringify({
            client_id: clientId,
            events: [{
              name: event.eventAction,
              params,
            }],
          }),
        });
    /* eslint-enable @typescript-eslint/naming-convention */
  };
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
 * Sends events to GA4.
 *
 * @param event Event to send.
 * @param dimensions Custom dimensions to include in the event.
 */
function sendGA4Event(
    event: UniversalAnalytics.FieldsObject,
    dimensions: Record<string, string>): void {
  void sendGA4EventReady.wait().then(
      (sendEvent) => sendEvent(event, dimensions));
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
  sendGA4Event: SendGA4Event;
  setMetricsEnabled: typeof setMetricsEnabled;
}
export {initGA, sendGAEvent, sendGA4Event, setMetricsEnabled};
