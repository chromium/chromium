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

const SCHEMA_VERSION = '3';

export interface GaBaseEvent {
  eventAction: string;
  eventCategory?: string;
  eventLabel?: string;
  eventValue?: number;
}

/**
 * All dimensions for GA metrics.
 *
 * The following two documents should also be updated when the dimensions is
 * updated.
 *
 * * Camera App PDD (Privacy Design Document): go/cca-metrics-pdd.
 * * CCA GA Events & Dimensions sheet: go/cca-metrics-schema.
 */
export enum GaMetricDimension {
  BOARD = 1,
  OS_VERSION = 2,
  // Obsolete 'sound' state.
  // SOUND = 3,
  MIRROR = 4,
  GRID = 5,
  TIMER = 6,
  MICROPHONE = 7,
  MAXIMIZED = 8,
  TALL_ORIENTATION = 9,
  RESOLUTION = 10,
  FPS = 11,
  INTENT_RESULT = 12,
  SHOULD_HANDLE_RESULT = 13,
  SHOULD_DOWN_SCALE = 14,
  IS_SECURE = 15,
  ERROR_NAME = 16,
  FILENAME = 17,
  FUNC_NAME = 18,
  LINE_NO = 19,
  COL_NO = 20,
  SHUTTER_TYPE = 21,
  IS_VIDEO_SNAPSHOT = 22,
  EVER_PAUSED = 23,
  SUPPORT_PAN = 24,
  SUPPORT_TILT = 25,
  SUPPORT_ZOOM = 26,
  // Obsolete
  // DOC_RESULT = 27,
  RECORD_TYPE = 28,
  GIF_RESULT = 29,
  DURATION = 30,
  SCHEMA_VERSION = 31,
  LAUNCH_TYPE = 32,
  DOC_FIX_TYPE = 33,
  RESOLUTION_LEVEL = 34,
  ASPECT_RATIO_SET = 35,
  DOC_PAGE_COUNT = 36,
  TIME_LAPSE_SPEED = 37,
  IS_TEST_IMAGE = 38,
  DEVICE_PIXEL_RATIO = 39,
  CAMERA_MODULE_ID = 40,
  WIFI_SECURITY_TYPE = 41,
}

export enum Ga4MetricDimension {
  ASPECT_RATIO_SET = 'aspect_ratio_set',
  BOARD = 'board',
  BROWSER_VERSION = 'browser_version',
  CAMERA_MODULE_ID = 'camera_module_id',
  COL_NO = 'col_no',
  DEVICE_PIXEL_RATIO = 'device_pixel_ratio',
  DOC_FIX_TYPE = 'doc_fix_type',
  DOC_PAGE_COUNT = 'doc_page_count',
  DURATION = 'duration',
  ERROR_NAME = 'error_name',
  EVENT_CATEGORY = 'event_category',
  EVENT_LABEL = 'event_label',
  EVER_PAUSED = 'ever_paused',
  FILENAME = 'filename',
  FPS = 'fps',
  FUNC_NAME = 'func_name',
  GIF_RESULT = 'gif_result',
  GRID = 'grid',
  INTENT_RESULT = 'intent_result',
  IS_SECURE = 'is_secure',
  IS_TEST_IMAGE = 'is_test_image',
  IS_VIDEO_SNAPSHOT = 'is_video_snapshot',
  LANGUAGE = 'language',
  LAUNCH_TYPE = 'launch_type',
  LINE_NO = 'line_no',
  MAXIMIZED = 'maximized',
  MEMORY_USAGE = 'memory_usage',
  MICROPHONE = 'microphone',
  MIRROR = 'mirror',
  OS_VERSION = 'os_version',
  RECORD_TYPE = 'record_type',
  RESOLUTION = 'resolution',
  RESOLUTION_LEVEL = 'resolution_level',
  SCHEMA_VERSION = 'schema_version',
  SCREEN_RESOLUTION = 'screen_resolution',
  SESSION_BEHAVIOR = 'session_behavior',
  SESSION_LENGTH = 'session_length',
  SHOULD_DOWN_SCALE = 'should_down_scale',
  SHOULD_HANDLE_RESULT = 'should_handle_result',
  SHUTTER_TYPE = 'shutter_type',
  SUPPORT_PAN = 'support_pan',
  SUPPORT_TILT = 'support_tilt',
  SUPPORT_ZOOM = 'support_zoom',
  TALL_ORIENTATION = 'tall_orientation',
  TIME_LAPSE_SPEED = 'time_lapse_speed',
  TIMER = 'timer',
  WIFI_SECURITY_TYPE = 'wifi_security_type',
}

export type Ga4EventParams =
    Partial<Record<Ga4MetricDimension, string>&{value: number}>;

type Ga4MeasurementProtocolEventParams = Ga4EventParams&{
  ['engagement_time_msec']: string,
  ['session_id']: string,
}

interface InitGaParams {
  id: string;
  baseDimensions: Map<GaMetricDimension, string>;
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
  baseParams: Ga4EventParams;
  clientId?: string;
  measurementId: string;
  sessionId?: string;
}

type Ga4Config = Required<InitGa4Params>;

/**
 * `ga4Enabled` and `measurementProtocolUrl` are used in tests.
 */
let ga4Enabled = true;
let measurementProtocolUrl = 'https://www.google-analytics.com/mp/collect';
let ga4Config: Ga4Config|null = null;
/**
 * Initializes GA4 for sending metrics.
 *
 * @param initParams The parameters to initialize GA4.
 * @param initParams.apiSecret The API secret to send events via Measurement
 * Protocol.
 * @param initParams.baseParams The event parameters that will be sent in every
 * event.
 * @param initParams.clientId The client ID for the current client for GA4.
 * @param initParams.measurementId The GA4 measurement ID.
 * @param initParams.sessionId The session ID in every event.
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
 * Sends an "end_session" event when CCA is closed or refreshed.
 */
function registerGa4EndSessionEvent(): void {
  window.addEventListener('unload', () => {
    sendGa4Event({
      name: 'end_session',
      eventParams: {
        [Ga4MetricDimension.SESSION_LENGTH]: window.performance.now().toFixed(),
      },
      beacon: true,
    });
  });
}

export interface MemoryUsageEventDimension {
  memoryUsage: number;
  sessionBehavior: number;
}
let memoryEventDimensions: MemoryUsageEventDimension|null = null;

/**
 * Sends a "memory_usage" event when CCA is closed or refreshed.
 */
function registerGa4MemoryUsageEvent(): void {
  window.addEventListener('unload', () => {
    if (memoryEventDimensions !== null) {
      const {memoryUsage, sessionBehavior} = memoryEventDimensions;
      sendGa4Event({
        name: 'memory_usage',
        eventParams: {
          [Ga4MetricDimension.MEMORY_USAGE]: String(memoryUsage),
          [Ga4MetricDimension.SESSION_BEHAVIOR]: String(sessionBehavior),
        },
        beacon: true,
      });
    }
  });
}

/**
 * Updates the memory usage and session behavior value which will be sent at the
 * end of the session.
 *
 * @param updatedValue New updated dimensions value to be set.
 */
function updateMemoryUsageEventDimensions(
    updatedValue: MemoryUsageEventDimension): void {
  memoryEventDimensions = updatedValue;
}

interface SendGaEventParams {
  baseEvent: GaBaseEvent;
  dimensions: Map<GaMetricDimension, string>;
}

/**
 * Sends an event to GA.
 *
 * @param params The parameters object.
 * @param params.baseEvent The basic event properties.
 * @param params.dimensions Custom dimensions of the event.
 */
function sendGaEvent({baseEvent, dimensions}: SendGaEventParams): void {
  assert(gaBaseDimensions !== null);
  const event: UniversalAnalytics.FieldsObject = {...baseEvent};
  const mergedDimensions: Array<[GaMetricDimension, string]> = [
    ...gaBaseDimensions,
    ...dimensions,
    [GaMetricDimension.DEVICE_PIXEL_RATIO, getDevicePixelRatio()],
    [GaMetricDimension.SCHEMA_VERSION, SCHEMA_VERSION],
  ];
  for (const [key, value] of mergedDimensions) {
    event[`dimension${key}`] = value;
  }
  window.ga('send', 'event', event);
}

interface SendGa4EventParams {
  name: string;
  eventParams: Ga4EventParams;
  beacon?: boolean;
}

/**
 * Sends an event to GA4.
 *
 * @param params The parameters object.
 * @param params.name The name of the event.
 * @param params.eventParams The event parameters (custom dimensions) of the
 *     event.
 * @param params.beacon Send the event via `navigator.sendBeacon`.
 */
function sendGa4Event({
  name,
  eventParams,
  beacon = false,
}: SendGa4EventParams): void {
  if (!ga4Enabled) {
    return;
  }
  const {apiSecret, baseParams, clientId, measurementId, sessionId} =
      assertExists(ga4Config);
  const params: Ga4MeasurementProtocolEventParams = {
    ...baseParams,
    ...eventParams,
    [Ga4MetricDimension.DEVICE_PIXEL_RATIO]: getDevicePixelRatio(),
    [Ga4MetricDimension.LANGUAGE]: navigator.language,
    [Ga4MetricDimension.SCHEMA_VERSION]: SCHEMA_VERSION,
    [Ga4MetricDimension.SCREEN_RESOLUTION]: getScreenResolution(),
    // Set '1' here as it's enough for GA4 to generate the metrics for n-day
    // active users and we don't want to reimplement how gtag.js calculate the
    // engagement time for each event.
    // See
    // https://developers.google.com/analytics/devguides/collection/protocol/ga4/sending-events?client_type=gtag#recommended_parameters_for_reports.
    ['engagement_time_msec']: '1',
    // Send this to create a session in GA4.
    // See
    // https://developers.google.com/analytics/devguides/collection/protocol/ga4/sending-events?client_type=gtag#recommended_parameters_for_reports.
    ['session_id']: sessionId,
  };

  const url = `${measurementProtocolUrl}?measurement_id=${
      measurementId}&api_secret=${apiSecret}`;
  const body = JSON.stringify({
    ['client_id']: clientId,
    events: [{name, params}],
  });

  if (beacon) {
    navigator.sendBeacon(url, body);
  } else {
    void fetch(url, {method: 'POST', body});
  }
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

/**
 * Sets if GA4 can send metrics.
 *
 * @param enabled True if the metrics is enabled.
 */
function setGa4Enabled(enabled: boolean): void {
  ga4Enabled = enabled;
}

function getDevicePixelRatio() {
  return window.devicePixelRatio.toFixed(2);
}

function getScreenResolution() {
  const {width, height} = window.screen;
  return `${width}x${height}`;
}

/**
 * Change the URL that measurement protocol used to send events. This should
 * be only called in tests.
 */
function setMeasurementProtocolUrl(url: string): void {
  measurementProtocolUrl = url;
}

export interface GaHelper {
  initGa: typeof initGa;
  initGa4: typeof initGa4;
  registerGa4EndSessionEvent: typeof registerGa4EndSessionEvent;
  registerGa4MemoryUsageEvent: typeof registerGa4MemoryUsageEvent;
  sendGaEvent: typeof sendGaEvent;
  sendGa4Event: typeof sendGa4Event;
  setGaEnabled: typeof setGaEnabled;
  setGa4Enabled: typeof setGa4Enabled;
  setMeasurementProtocolUrl: typeof setMeasurementProtocolUrl;
  updateMemoryUsageEventDimensions: typeof updateMemoryUsageEventDimensions;
}
export {
  initGa,
  initGa4,
  registerGa4EndSessionEvent,
  registerGa4MemoryUsageEvent,
  sendGaEvent,
  sendGa4Event,
  setGaEnabled,
  setGa4Enabled,
  setMeasurementProtocolUrl,
  updateMemoryUsageEventDimensions,
};
