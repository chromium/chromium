// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {Intent} from './intent.js';
import * as Comlink from './lib/comlink.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as state from './state.js';
import {
  Facing,
  Mode,
  PerfEvent,
  PerfInformation,
  Resolution,
} from './type.js';
import {GAHelperInterface} from './untrusted_helper_interfaces.js';
import * as util from './util.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * The tracker ID of the GA metrics.
 */
const GA_ID = 'UA-134822711-1';

let baseDimen: Map<number, string|number>|null = null;

const ready = new WaitableEvent();

const gaHelper = util.createUntrustedJSModule<GAHelperInterface>(
    '/js/untrusted_ga_helper.js');

/**
 * Send the event to GA backend.
 * @param event The event to send.
 * @param dimen Optional object contains dimension information.
 */
async function sendEvent(
    event: UniversalAnalytics.FieldsObject, dimen?: Map<number, unknown>) {
  const assignDimension = (e, d) => {
    for (const [key, value] of d.entries()) {
      e[`dimension${key}`] = value;
    }
  };

  assert(baseDimen !== null);
  assignDimension(event, baseDimen);
  if (dimen !== undefined) {
    assignDimension(event, dimen);
  }

  await ready.wait();

  // This value reflects the logging consent option in OS settings.
  const canSendMetrics =
      await ChromeHelper.getInstance().isMetricsAndCrashReportingEnabled();
  if (canSendMetrics) {
    (await gaHelper).sendGAEvent(event);
  }
}

/**
 * Set if the metrics is enabled. Note that the metrics will only be sent if it
 * is enabled AND the logging consent option is enabled in OS settings.
 * @param enabled True if the metrics is enabled.
 */
export async function setMetricsEnabled(enabled: boolean): Promise<void> {
  await ready.wait();
  await (await gaHelper).setMetricsEnabled(GA_ID, enabled);
}

const SCHEMA_VERSION = 2;

/**
 * Initializes metrics with parameters.
 */
export async function initMetrics(): Promise<void> {
  const board = loadTimeData.getBoard();
  const boardName = /^(x86-)?(\w*)/.exec(board)[0];
  const match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
  const osVer = match ? match[1] : '';
  baseDimen = new Map<number, string|number>([
    [1, boardName],
    [2, osVer],
    [31, SCHEMA_VERSION],
  ]);

  const GA_LOCAL_STORAGE_KEY = 'google-analytics.analytics.user-id';
  const clientId = localStorage.getString(GA_LOCAL_STORAGE_KEY);

  const setClientId = (id) => {
    localStorage.set(GA_LOCAL_STORAGE_KEY, id);
  };

  await (await gaHelper).initGA(GA_ID, clientId, Comlink.proxy(setClientId));
  ready.signal();
}

/**
 * Types of different ways to launch CCA.
 */
export enum LaunchType {
  DEFAULT = 'default',
  ASSISTANT = 'assistant',
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
        [32, launchType],
      ]));
}

/**
 * Types of intent result dimension.
 */
export enum IntentResultType {
  NOT_INTENT = '',
  CANCELED = 'canceled',
  CONFIRMED = 'confirmed',
}

/**
 * Types of document scanning result dimension.
 */
export enum DocResultType {
  NOT_DOCUMENT = '',
  CANCELED = 'canceled',
  SAVE_AS_PHOTO = 'save-as-photo',
  SAVE_AS_PDF = 'save-as-pdf',
  SHARE = 'share',
}

/**
 * Types of user interaction with fix document page.
 */
export enum DocFixType {
  NONE = 0,
  NO_FIX = 1,
  FIX_ROTATION = 2,
  FIX_POSITION = 3,
  FIX_BOTH = 4,
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
}

/**
 * Types of different ways to trigger shutter button.
 */
export enum ShutterType {
  UNKNOWN = 'unknown',
  MOUSE = 'mouse',
  KEYBOARD = 'keyboard',
  TOUCH = 'touch',
  VOLUME_KEY = 'volume-key',
  ASSISTANT = 'assistant',
}

/**
 * Parameters of capture metrics event.
 */
export interface CaptureEventParam {
  /** Camera facing of the capture. */
  facing: Facing;
  /** Length of duration for captured motion result in milliseconds. */
  duration?: number;
  /** Capture resolution. */
  resolution: Resolution;
  intentResult?: IntentResultType;
  shutterType: ShutterType;
  /** Whether the event is for video snapshot. */
  isVideoSnapshot?: boolean;
  /** Whether the video have ever paused and resumed in the recording. */
  everPaused?: boolean;
  docResult?: DocResultType;
  docFixType?: DocFixType;
  gifResult?: GifResultType;
  recordType?: RecordType;
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
  docResult = DocResultType.NOT_DOCUMENT,
  docFixType,
  recordType = RecordType.NOT_RECORDING,
  gifResult = GifResultType.NOT_GIF_RESULT,
}: CaptureEventParam): void {
  const condState =
      (states: state.StateUnion[], cond?: state.StateUnion, strict?: boolean):
          string => {
            // Return the first existing state among the given states only if
            // there is no gate condition or the condition is met.
            const prerequisite = !cond || state.get(cond);
            if (strict && !prerequisite) {
              return '';
            }
            return prerequisite && states.find((s) => state.get(s)) || 'n/a';
          };

  const State = state.State;
  sendEvent(
      {
        eventCategory: 'capture',
        eventAction: condState(Object.values(Mode)),
        eventLabel: facing,
        eventValue: duration,
      },
      new Map<number, unknown>([
        // Skips 3rd dimension for obsolete 'sound' state.
        [4, condState([State.MIRROR])],
        [
          5,
          condState(
              [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID),
        ],
        [6, condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER)],
        [7, condState([State.MIC], Mode.VIDEO, true)],
        [8, condState([State.MAX_WND])],
        [9, condState([State.TALL])],
        [10, resolution.toString()],
        [11, condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true)],
        [12, intentResult],
        [21, shutterType],
        [22, isVideoSnapshot],
        [23, everPaused],
        [27, docResult],
        [28, recordType],
        [29, gifResult],
        [30, duration],
        // This is included in baseDimen.
        // [31, SCHEMA_VERSION]
        [32, docFixType ?? ''],
      ]));
}


/**
 * Parameters for logging perf event.
 */
interface PerfEventParam {
  /** Target event type. */
  event: PerfEvent;
  /** Duration of the event in ms. */
  duration: number;
  /** Optional information for the event. */
  perfInfo?: PerfInformation;
}

/**
 * Sends perf type event.
 */
export function sendPerfEvent({event, duration, perfInfo = {}}: PerfEventParam):
    void {
  const resolution = perfInfo['resolution'] || '';
  const facing = perfInfo['facing'] || '';
  sendEvent(
      {
        eventCategory: 'perf',
        eventAction: event,
        eventLabel: facing,
        // Round the duration here since GA expects that the value is an
        // integer. Reference:
        // https://support.google.com/analytics/answer/1033068
        eventValue: Math.round(duration),
      },
      new Map([
        [10, `${resolution}`],
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
  const getBoolValue = (b) => b ? '1' : '0';
  sendEvent(
      {
        eventCategory: 'intent',
        eventAction: mode,
        eventLabel: result,
      },
      new Map([
        [12, result],
        [13, getBoolValue(shouldHandleResult)],
        [14, getBoolValue(shouldDownScale)],
        [15, getBoolValue(isSecure)],
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
        [16, errorName],
        [17, fileName],
        [18, funcName],
        [19, lineNo],
        [20, colNo],
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
        [24, capabilities.pan],
        [25, capabilities.tilt],
        [26, capabilities.zoom],
      ]));
}
