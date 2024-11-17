// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';


const SCHEMA_VERSION = '3';

/**
 * All dimensions for GA metrics.
 *
 * The following two documents should also be updated when the dimensions is
 * updated.
 *
 * * Camera App PDD (Privacy Design Document): go/cca-metrics-pdd.
 * * CCA GA Events & Dimensions sheet: go/cca-metrics-schema.
 */
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
  IS_PRIMARY_LANGUAGE = 'is_primary_language',
  IS_SECURE = 'is_secure',
  IS_TEST_IMAGE = 'is_test_image',
  IS_VIDEO_SNAPSHOT = 'is_video_snapshot',
  LANGUAGE = 'language',
  LAUNCH_TYPE = 'launch_type',
  LINE_COUNT = 'line_count',
  LINE_NO = 'line_no',
  MAXIMIZED = 'maximized',
  MEMORY_USAGE = 'memory_usage',
  MICROPHONE = 'microphone',
  MIRROR = 'mirror',
  OS_VERSION = 'os_version',
  PRESSURE = 'pressure',
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
  WORD_COUNT = 'word_count',
  ZOOM_RATIO = 'zoom_ratio',
}

export type Ga4EventParams =
    Partial<Record<Ga4MetricDimension, string>&{value: number}>;

type Ga4MeasurementProtocolEventParams = Ga4EventParams&{
  ['engagement_time_msec']: string,
  ['session_id']: string,
};

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
const measurementProtocolUrl = 'https://www.google-analytics.com/mp/collect';
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

export interface GaHelper {
  initGa4: typeof initGa4;
  registerGa4EndSessionEvent: typeof registerGa4EndSessionEvent;
  registerGa4MemoryUsageEvent: typeof registerGa4MemoryUsageEvent;
  sendGa4Event: typeof sendGa4Event;
  setGa4Enabled: typeof setGa4Enabled;
  updateMemoryUsageEventDimensions: typeof updateMemoryUsageEventDimensions;
}
export {
  initGa4,
  registerGa4EndSessionEvent,
  registerGa4MemoryUsageEvent,
  sendGa4Event,
  setGa4Enabled,
  updateMemoryUsageEventDimensions,
};
