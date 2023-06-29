// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import {Intent} from './intent.js';
import * as Comlink from './lib/comlink.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as state from './state.js';
import {State} from './state.js';
import {
  AspectRatioSet,
  Facing,
  LocalStorageKey,
  Mode,
  PerfEvent,
  PerfInformation,
  PhotoResolutionLevel,
  Resolution,
  VideoResolutionLevel,
} from './type.js';
import {getGaHelper} from './untrusted_scripts.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * The tracker ID of the GA metrics and the measurement ID of GA4 events. Make
 * sure to set `PRODUCTION` to `false` when developing/debugging metrics. See
 * Debugging section in go/cros-camera:dd:cca-ga-migration.
 */
const PRODUCTION = true;
const GA_ID = PRODUCTION ? 'UA-134822711-1' : 'UA-134822711-2';
const GA4_ID = PRODUCTION ? 'G-TRQS261G6E' : 'G-J03LBPJBGD';
const GA4_API_SECRET =
    PRODUCTION ? '0Ir88y9HQtiwnchvaIzZ3Q' : 'WE_zBPUQTGefdXpHl25-ig';

const ready = new WaitableEvent();

export interface BaseEvent {
  eventAction: string;
  eventCategory?: string;
  eventLabel?: string;
  eventValue?: number;
}

/**
 * Send the event to GA backend.
 *
 * @param event The event to send.
 * @param dimensions Optional object contains dimension information.
 */
async function sendEvent(
    event: BaseEvent, dimensions: Map<MetricDimension, string> = new Map()) {
  if (event.eventValue !== undefined && !Number.isInteger(event.eventValue)) {
    // Round the duration here since GA expects that the value is an
    // integer. Reference:
    // https://support.google.com/analytics/answer/1033068
    event.eventValue = Math.round(event.eventValue);
  }

  await ready.wait();

  // This value reflects the logging consent option in OS settings.
  const canSendMetrics = !PRODUCTION ||
      await ChromeHelper.getInstance().isMetricsAndCrashReportingEnabled();
  if (canSendMetrics) {
    await Promise.all([
      sendGaEvent(event, dimensions),
      sendGa4Event(event, dimensions),
    ]);
  }
}

async function sendGaEvent(
    baseEvent: BaseEvent, dimensions: Map<MetricDimension, string>) {
  const gaHelper = await getGaHelper();
  await gaHelper.sendGaEvent(baseEvent, dimensions);
}

async function sendGa4Event(
    event: BaseEvent, dimensions: Map<MetricDimension, string>) {
  const params: Record<string, number|string> = {};
  if (event.eventCategory !== undefined) {
    params['event_category'] = event.eventCategory;
  }
  if (event.eventLabel !== undefined) {
    params['event_label'] = event.eventLabel;
  }
  if (event.eventValue !== undefined) {
    params['value'] = event.eventValue;
  }
  // TODO(b/267265966): Use enum as event parameter keys. Simply map lower cased
  // enum keys from UA to parameter keys now.
  for (const [enumKey, value] of dimensions) {
    const key = MetricDimension[enumKey].toLowerCase();
    params[key] = value;
  }
  const gaHelper = await getGaHelper();
  const name = event.eventAction.replaceAll('-', '_');
  await gaHelper.sendGa4Event(name, params);
}

/**
 * Set if the metrics is enabled. Note that the metrics will only be sent if it
 * is enabled AND the logging consent option is enabled in OS settings.
 *
 * @param enabled True if the metrics is enabled.
 */
export async function setGaEnabled(enabled: boolean): Promise<void> {
  await ready.wait();
  await (await getGaHelper()).setGaEnabled(GA_ID, enabled);
}

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
export enum MetricDimension {
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
}

/**
 * Initializes metrics with parameters.
 */
export async function initMetrics(): Promise<void> {
  const board = assertExists(/^(x86-)?(\w*)/.exec(loadTimeData.getBoard()))[0];
  const osVer = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/)?.[1] ?? '';
  const isTestImage = loadTimeData.getIsTestImage();

  async function initGa() {
    const baseDimensions = new Map([
      [MetricDimension.BOARD, board],
      [MetricDimension.OS_VERSION, osVer],
      [MetricDimension.SCHEMA_VERSION, SCHEMA_VERSION],
      [MetricDimension.IS_TEST_IMAGE, boolToIntString(isTestImage)],
    ]);

    const clientId = localStorage.getString(LocalStorageKey.GA_USER_ID);
    function setClientId(id: string) {
      localStorage.set(LocalStorageKey.GA_USER_ID, id);
    }
    const gaHelper = await getGaHelper();
    return gaHelper.initGa(
        {
          id: GA_ID,
          clientId,
          baseDimensions,
        },
        Comlink.proxy(setClientId));
  }

  async function initGa4() {
    // TODO(b/267265966): Use enum as event parameter keys.
    const baseParams = {
      board,
      ['os_version']: osVer,
      ['schema_version']: SCHEMA_VERSION,
      ['is_test_image']: boolToIntString(isTestImage),
      ['browser_version']: loadTimeData.getBrowserVersion(),
    };

    const clientId = localStorage.getString(LocalStorageKey.GA4_CLIENT_ID);
    function setClientId(id: string) {
      localStorage.set(LocalStorageKey.GA4_CLIENT_ID, id);
    }
    const gaHelper = await getGaHelper();
    return gaHelper.initGa4(
        {
          apiSecret: GA4_API_SECRET,
          baseParams,
          clientId,
          measurementId: GA4_ID,
        },
        Comlink.proxy(setClientId));
  }

  await Promise.all([
    initGa(),
    initGa4(),
  ]);
  ready.signal();
}

/**
 * Types of different ways to launch CCA.
 */
export enum LaunchType {
  ASSISTANT = 'assistant',
  DEFAULT = 'default',
}

/**
 * Parameters for logging launch event. |launchType| stands for how CCA is
 * launched.
 */
export interface LaunchEventParam {
  launchType: LaunchType;
}

/**
 * Sends launch type event.
 */
export function sendLaunchEvent({launchType}: LaunchEventParam): void {
  sendEvent(
      {
        eventCategory: 'launch',
        eventAction: 'start',
        eventLabel: '',
      },
      new Map([
        [MetricDimension.LAUNCH_TYPE, launchType],
      ]));
}

/**
 * Types of intent result dimension.
 */
export enum IntentResultType {
  CANCELED = 'canceled',
  CONFIRMED = 'confirmed',
  NOT_INTENT = '',
}

/**
 * Types of gif recording result dimension.
 */
export enum GifResultType {
  NOT_GIF_RESULT = 0,
  RETAKE = 1,
  SHARE = 2,
  SAVE = 3,
}

/**
 * Types of recording in video mode.
 */
export enum RecordType {
  NOT_RECORDING = 0,
  NORMAL_VIDEO = 1,
  GIF = 2,
  TIME_LAPSE = 3,
}

/**
 * Types of different ways to trigger shutter button.
 */
export enum ShutterType {
  ASSISTANT = 'assistant',
  KEYBOARD = 'keyboard',
  MOUSE = 'mouse',
  TOUCH = 'touch',
  UNKNOWN = 'unknown',
  VOLUME_KEY = 'volume-key',
}

/**
 * Parameters of capture metrics event.
 */
export interface CaptureEventParam {
  /**
   * Camera facing of the capture.
   */
  facing: Facing;

  /**
   * Length of duration for captured motion result in milliseconds.
   */
  duration?: number;

  /**
   * Capture resolution.
   */
  resolution: Resolution;

  intentResult?: IntentResultType;
  shutterType: ShutterType;

  /**
   * Whether the event is for video snapshot.
   */
  isVideoSnapshot?: boolean;

  /**
   * Whether the video have ever paused and resumed in the recording.
   */
  everPaused?: boolean;

  gifResult?: GifResultType;
  recordType?: RecordType;

  resolutionLevel: PhotoResolutionLevel|VideoResolutionLevel;
  aspectRatioSet: AspectRatioSet;

  timeLapseSpeed?: number;
}

/**
 * Sends capture type event.
 */
export function sendCaptureEvent({
  facing,
  duration = 0,
  resolution,
  intentResult = IntentResultType.NOT_INTENT,
  shutterType,
  isVideoSnapshot = false,
  everPaused = false,
  recordType = RecordType.NOT_RECORDING,
  gifResult = GifResultType.NOT_GIF_RESULT,
  resolutionLevel,
  aspectRatioSet,
  timeLapseSpeed = 0,
}: CaptureEventParam): void {
  function condState(
      states: state.StateUnion[],
      cond?: state.StateUnion,
      strict = false,
      ): string {
    // Return the first existing state among the given states only if
    // there is no gate condition or the condition is met.
    const prerequisite = cond === undefined || state.get(cond);
    if (!prerequisite) {
      return strict ? '' : 'n/a';
    }
    return states.find((s) => state.get(s)) ?? 'n/a';
  }

  sendEvent(
      {
        eventCategory: 'capture',
        eventAction: condState(Object.values(Mode)),
        eventLabel: facing,
        eventValue: duration,
      },
      new Map([
        // Skips 3rd dimension for obsolete 'sound' state.
        [MetricDimension.MIRROR, condState([State.MIRROR])],
        [
          MetricDimension.GRID,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID),
        ],
        [
          MetricDimension.TIMER,
          condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER),
        ],
        [MetricDimension.MICROPHONE, condState([State.MIC], Mode.VIDEO, true)],
        [MetricDimension.MAXIMIZED, condState([State.MAX_WND])],
        [MetricDimension.TALL_ORIENTATION, condState([State.TALL])],
        [MetricDimension.RESOLUTION, resolution.toString()],
        [
          MetricDimension.FPS,
          condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true),
        ],
        [MetricDimension.INTENT_RESULT, intentResult],
        [MetricDimension.SHUTTER_TYPE, shutterType],
        [MetricDimension.IS_VIDEO_SNAPSHOT, boolToIntString(isVideoSnapshot)],
        [MetricDimension.EVER_PAUSED, boolToIntString(everPaused)],
        [MetricDimension.RECORD_TYPE, String(recordType)],
        [MetricDimension.GIF_RESULT, String(gifResult)],
        [MetricDimension.DURATION, String(duration)],
        [MetricDimension.RESOLUTION_LEVEL, resolutionLevel],
        [MetricDimension.ASPECT_RATIO_SET, String(aspectRatioSet)],
        [MetricDimension.TIME_LAPSE_SPEED, String(timeLapseSpeed)],
      ]));
}


/**
 * Parameters for logging perf event.
 */
interface PerfEventParam {
  /**
   * Target event type.
   */
  event: PerfEvent;

  /**
   * Duration of the event in ms.
   */
  duration: number;

  /**
   * Optional information for the event.
   */
  perfInfo?: PerfInformation;
}

/**
 * Sends perf type event.
 */
export function sendPerfEvent({event, duration, perfInfo = {}}: PerfEventParam):
    void {
  const resolution = perfInfo.resolution ?? '';
  const facing = perfInfo.facing ?? '';
  sendEvent(
      {
        eventCategory: 'perf',
        eventAction: event,
        eventLabel: facing,
        eventValue: duration,
      },
      new Map([
        [MetricDimension.RESOLUTION, `${resolution}`],
      ]));
}

/**
 * See Intent class in intent.js for the descriptions of each field.
 */
export interface IntentEventParam {
  intent: Intent;
  result: IntentResultType;
}

/**
 * Sends intent type event.
 */
export function sendIntentEvent({intent, result}: IntentEventParam): void {
  const {mode, shouldHandleResult, shouldDownScale, isSecure} = intent;
  sendEvent(
      {
        eventCategory: 'intent',
        eventAction: mode,
        eventLabel: result,
      },
      new Map([
        [MetricDimension.INTENT_RESULT, result],
        [
          MetricDimension.SHOULD_HANDLE_RESULT,
          boolToIntString(shouldHandleResult),
        ],
        [MetricDimension.SHOULD_DOWN_SCALE, boolToIntString(shouldDownScale)],
        [MetricDimension.IS_SECURE, boolToIntString(isSecure)],
      ]));
}

export interface ErrorEventParam {
  type: string;
  level: string;
  errorName: string;
  fileName: string;
  funcName: string;
  lineNo: string;
  colNo: string;
}

/**
 * Sends error type event.
 */
export function sendErrorEvent(
    {type, level, errorName, fileName, funcName, lineNo, colNo}:
        ErrorEventParam): void {
  sendEvent(
      {
        eventCategory: 'error',
        eventAction: type,
        eventLabel: level,
      },
      new Map([
        [MetricDimension.ERROR_NAME, errorName],
        [MetricDimension.FILENAME, fileName],
        [MetricDimension.FUNC_NAME, funcName],
        [MetricDimension.LINE_NO, lineNo],
        [MetricDimension.COL_NO, colNo],
      ]));
}

/**
 * Sends the barcode enabled event.
 */
export function sendBarcodeEnabledEvent(): void {
  sendEvent({
    eventCategory: 'barcode',
    eventAction: 'enable',
  });
}

/**
 * Types of the decoded barcode content.
 */
export enum BarcodeContentType {
  TEXT = 'text',
  URL = 'url',
}

interface BarcodeDetectedEventParam {
  contentType: BarcodeContentType;
}

/**
 * Sends the barcode detected event.
 */
export function sendBarcodeDetectedEvent(
    {contentType}: BarcodeDetectedEventParam): void {
  sendEvent({
    eventCategory: 'barcode',
    eventAction: 'detect',
    eventLabel: contentType,
  });
}

/**
 * Sends the open ptz panel event.
 */
export function sendOpenPTZPanelEvent(
    capabilities: {pan: boolean, tilt: boolean, zoom: boolean}): void {
  sendEvent(
      {
        eventCategory: 'ptz',
        eventAction: 'open-panel',
      },
      new Map([
        [MetricDimension.SUPPORT_PAN, boolToIntString(capabilities.pan)],
        [MetricDimension.SUPPORT_TILT, boolToIntString(capabilities.tilt)],
        [MetricDimension.SUPPORT_ZOOM, boolToIntString(capabilities.zoom)],
      ]));
}

export enum DocScanFixType {
  NONE = 0,
  CORNER = 0b1,
  ROTATION = 0b10,
}

export enum DocScanResultActionType {
  CANCEL = 'cancel',
  SAVE_AS_PDF = 'save-as-pdf',
  SAVE_AS_PHOTO = 'save-as-photo',
  SHARE = 'share',
}

/**
 * Sends document scanning result event. The actions will either remove all
 * pages (cancel) or generate a file from pages (save/share).
 */
export function sendDocScanResultEvent(
    action: DocScanResultActionType,
    fixType: DocScanFixType,
    fixCount: number,
    pageCount: number,
    ): void {
  sendEvent(
      {
        eventCategory: 'doc-scan',
        eventAction: action,
        eventValue: fixCount,
      },
      new Map([
        [MetricDimension.DOC_FIX_TYPE, String(fixType)],
        [MetricDimension.DOC_PAGE_COUNT, String(pageCount)],
      ]));
}

export enum DocScanActionType {
  ADD_PAGE = 'add-page',
  DELETE_PAGE = 'delete-page',
  FIX = 'fix',
}

/**
 * Sends document scanning event.
 */
export function sendDocScanEvent(action: DocScanActionType): void {
  sendEvent({
    eventCategory: 'doc-scan',
    eventAction: action,
  });
}

export enum LowStorageActionType {
  MANAGE_STORAGE_AUTO_STOP = 'manage-storage-auto-stop',
  MANAGE_STORAGE_CANNOT_START = 'manage-storage-cannot-start',
  SHOW_AUTO_STOP_DIALOG = 'show-auto-stop-dialog',
  SHOW_CANNOT_START_DIALOG = 'show-cannot-start-dialog',
  SHOW_WARNING_MSG = 'show-warning-msg',
}

/**
 * Sends low-storage handling event.
 */
export function sendLowStorageEvent(action: LowStorageActionType): void {
  sendEvent({
    eventCategory: 'low-storage',
    eventAction: action,
  });
}

function boolToIntString(b: boolean) {
  return b ? '1' : '0';
}
