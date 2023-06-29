// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import {BaseEvent, MetricDimension} from './metrics.js';

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

interface InitGaParams {
  id: string;
  baseDimensions: Map<MetricDimension, string>;
  clientId: string;
}
let gaBaseDimensions: InitGaParams['baseDimensions']|null = null;
/**
 * Initializes GA for sending metrics.
 *
 * @param initParams The parameters to initialize GA.
 * @param initParams.id The GA tracker ID to send events.
 * @param initParams.baseDimensions The base dimensions that will be sent in
 * every event.
 * @param initParams.clientId The client ID for the current client for GA.
 * @param setClientId The callback to store client id for GA.
 */
function initGa(
    initParams: InitGaParams, setClientId: (clientId: string) => void): void {
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

  const {id, baseDimensions, clientId} = initParams;
  gaBaseDimensions = baseDimensions;

  window.ga('create', id, {
    storage: 'none',
    clientId: clientId,
  });

  window.ga((tracker?: UniversalAnalytics.Tracker) => {
    assert(tracker !== undefined);
    setClientId(tracker.get('clientId'));
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

interface InitGa4Params {
  apiSecret: string;
  baseParams: Record<string, number|string>;
  clientId?: string;
  measurementId: string;
  sessionId?: string;
}

type Ga4Config = Required<InitGa4Params>;

let ga4Config: Ga4Config|null = null;
/**
 * Initializes GA4 for sending metrics.
 *
 * @param initParams The parameters to initialize GA4.
 * @param initParams.apiSecret The API secret to send events via Measurement
 * Protocol.
 * @param initParams.baseParams The event parameters that will be sent in every
 * event.
 * @param initParams.measurementId The GA4 measurement ID.
 * @param initParams.clientId The client ID for the current client for GA4.
 * @param setClientId The callback to store client id for GA4.
 */
function initGa4(
    initParams: InitGa4Params, setClientId: (clientId: string) => void): void {
  let {clientId, sessionId} = initParams;
  if (clientId === undefined || clientId === '') {
    clientId = generateGa4ClientId();
  }
  setClientId(clientId);
  if (sessionId === undefined || sessionId === '') {
    sessionId = generateGa4SessionId();
  }
  ga4Config = {
    ...initParams,
    clientId,
    sessionId,
  };
}

function generateGa4ClientId() {
  const randomNumber = Math.round(Math.random() * 0x7fffffff);
  const timestamp = Math.round(Date.now() / 1000);
  return `${randomNumber}.${timestamp}`;
}

function generateGa4SessionId() {
  return String(Date.now());
}

/**
 * Sends an event to GA.
 *
 * @param baseEvent The basic event properties.
 * @param eventDimensions Custom dimensions of the event.
 */
function sendGaEvent(
    baseEvent: BaseEvent, eventDimensions: Map<MetricDimension, string>): void {
  assert(gaBaseDimensions !== null);
  const event: UniversalAnalytics.FieldsObject = {...baseEvent};
  for (const [key, value] of [...gaBaseDimensions, ...eventDimensions]) {
    event[`dimension${key}`] = value;
  }
  window.ga('send', 'event', event);
}

/**
 * Sends an event to GA4.
 *
 * @param name The name of the event.
 * @param eventParams The event parameters (custom dimensions) of the event.
 */
function sendGa4Event(
    name: string, eventParams: Record<string, number|string>): void {
  const {apiSecret, baseParams, clientId, measurementId, sessionId} =
      assertExists(ga4Config);
  // TODO(b/267265966): GA4 uses `engagement_time_msec` and `session_id` to
  // calculate user activity. Remove these parameters as they are sent
  // automatically by gtag.js.
  const params = {
    ...baseParams,
    ...eventParams,
    // Set '1' here as it's enough for GA4 to generate the metrics for n-day
    // active users and we don't want to reimplement how gtag.js calculate the
    // engagement time for each event.
    ['engagement_time_msec']: '1',
    ['session_id']: sessionId,
  };
  void fetch(
      `https://www.google-analytics.com/mp/collect?measurement_id=${
          measurementId}&api_secret=${apiSecret}`,
      {
        method: 'POST',
        body: JSON.stringify({
          ['client_id']: clientId,
          events: [{name, params}],
        }),
      });
}

/**
 * Sets if GA can send metrics.
 *
 * @param id The GA tracker ID.
 * @param enabled True if the metrics is enabled.
 */
function setGaEnabled(id: string, enabled: boolean): void {
  window[`ga-disable-${id}`] = !enabled;
}

export interface GaHelper {
  initGa: typeof initGa;
  initGa4: typeof initGa4;
  sendGaEvent: typeof sendGaEvent;
  sendGa4Event: typeof sendGa4Event;
  setGaEnabled: typeof setGaEnabled;
}
export {initGa, initGa4, sendGaEvent, sendGa4Event, setGaEnabled};
