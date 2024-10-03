// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import {Intent} from './intent.js';
import * as comlink from './lib/comlink.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as mojoType from './mojo/type.js';
import * as mojoTypeUtils from './mojo/type_utils.js';
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
import {
  Ga4EventParams,
  Ga4MetricDimension,
  getGaHelper,
  MemoryUsageEventDimension,
} from './untrusted_scripts.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * The tracker ID of the GA metrics and the measurement ID of GA4 events. Make
 * sure to set `PRODUCTION` to `false` when developing/debugging metrics. See
 * Debugging section in go/cros-camera:dd:cca-ga-migration.
 */
const PRODUCTION = true;
const GA4_ID = PRODUCTION ? 'G-TRQS261G6E' : 'G-J03LBPJBGD';
const GA4_API_SECRET =
    PRODUCTION ? '0Ir88y9HQtiwnchvaIzZ3Q' : 'WE_zBPUQTGefdXpHl25-ig';

const ready = new WaitableEvent();

// This is used to send events via CrOS Events.
let eventsSender: mojoType.EventsSenderRemote|null = null;

/**
 * Sends the event to GA backend.
 *
 * @param eventName The name of the event.
 * @param eventParams Optional object contains dimension information.
 */
function sendEvent(eventName: string, eventParams: Ga4EventParams = {}) {
  if (eventParams.value !== undefined && !Number.isInteger(eventParams.value)) {
    // Round the duration here since GA expects that the value is an
    // integer. Reference:
    // https://support.google.com/analytics/answer/1033068
    eventParams.value = Math.round(eventParams.value);
  }

  // GA4 only accepts '_' in event names.
  const name = eventName.replaceAll('-', '_');

  // No caller use the returned promise since metrics sending should not block
  // the code.
  void (async () => {
    await ready.wait();

    if (await checkCanSendMetrics()) {
      const gaHelper = await getGaHelper();
      await gaHelper.sendGa4Event({name, eventParams});
    }
  })();
}

/**
 * Sets if the metrics is enabled. Note that the metrics will only be sent if it
 * is enabled AND the logging consent option is enabled in OS settings.
 *
 * @param enabled True if the metrics is enabled.
 */
export async function setEnabled(enabled: boolean): Promise<void> {
  await ready.wait();
  const gaHelper = await getGaHelper();
  await gaHelper.setGa4Enabled(enabled);
}

/**
 * Initializes GA and GA4 with parameters like property ID, client ID, and
 * custom dimensions from `loadTimeData`.
 */
export async function initMetrics(): Promise<void> {
  const board = assertExists(/^(x86-)?(\w*)/.exec(loadTimeData.getBoard()))[0];
  const isTestImage = loadTimeData.getIsTestImage();
  const gaHelper = await getGaHelper();

  function initGa4() {
    const baseParams = {
      [Ga4MetricDimension.BOARD]: board,
      [Ga4MetricDimension.IS_TEST_IMAGE]: boolToIntString(isTestImage),
      [Ga4MetricDimension.BROWSER_VERSION]: loadTimeData.getBrowserVersion(),
      [Ga4MetricDimension.OS_VERSION]: loadTimeData.getOsVersion(),
    };

    const clientId = localStorage.getString(LocalStorageKey.GA4_CLIENT_ID);
    function setClientId(id: string) {
      localStorage.set(LocalStorageKey.GA4_CLIENT_ID, id);
    }
    return gaHelper.initGa4(
        {
          apiSecret: GA4_API_SECRET,
          baseParams,
          clientId,
          measurementId: GA4_ID,
        },
        comlink.proxy(setClientId));
  }

  // GA_IDs are refreshed every 90 days cycle according to GA_ID_REFRESH_TIME.
  // If GA_ID_REFRESH_TIME does not exist or is outdated, updates
  // GA_ID_REFRESH_TIME and removes outdated GA_USER_ID and GA4_CLIENT_ID to
  // have the new IDs.
  const timeNow = Date.now();
  const dayInMs = 1000 * 60 * 60 * 24;
  let refreshTime = localStorage.getNumber(LocalStorageKey.GA_ID_REFRESH_TIME);
  // Assign the first |refreshTime| uniformly in today+[1,90] days.
  // This solves an initial launch problem by avoiding that all Chromebooks do a
  // synchronized refresh 90 days after launch.
  if (refreshTime === 0) {
    const randomInt = Math.floor(Math.random() * 90) + 1;
    refreshTime = timeNow + randomInt * dayInMs;
  } else if (refreshTime <= timeNow) {
    localStorage.remove(LocalStorageKey.GA_USER_ID);
    localStorage.remove(LocalStorageKey.GA4_CLIENT_ID);
    const cycle = 90 * dayInMs;
    const cycleCount = Math.floor((timeNow - refreshTime) / cycle) + 1;
    refreshTime += cycle * cycleCount;
  }
  localStorage.set(LocalStorageKey.GA_ID_REFRESH_TIME, refreshTime);

  await initGa4();
  // TODO(b/286511762): Monitor consent option to enable/disable sending
  // metrics. Since end_session event is sent when the window is unloaded, we
  // don't have time to read the value from `checkCanSendMetrics()`. Currently,
  // we check the consent option on register instead of send.
  if (await checkCanSendMetrics()) {
    await Promise.all([
      gaHelper.registerGa4EndSessionEvent(),
      gaHelper.registerGa4MemoryUsageEvent(),
    ]);
  }
  ready.signal();
}

/**
 * Types of different ways to launch CCA.
 */
export enum LaunchType {
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
  sendEvent('start', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'launch',
    [Ga4MetricDimension.LAUNCH_TYPE]: launchType,
  });
  void (async () => {
    (await getEventsSender()).sendStartSessionEvent({
      launchType: mojoTypeUtils.convertLaunchTypeToMojo(launchType),
    });
  })();
}

async function getEventsSender(): Promise<mojoType.EventsSenderRemote> {
  if (eventsSender === null) {
    eventsSender = await ChromeHelper.getInstance().getEventsSender();
  }
  return eventsSender;
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

  /**
   * Zoom ratio when capturing. The value is 1 if zoomed-out, or the camera does
   * not support digital zoom.
   */
  zoomRatio?: number;
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
  zoomRatio = 1.0,
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

  const mode = condState(Object.values(Mode));
  const mirrorState = condState([State.MIRROR]);
  const gridType = condState(
      [State.GRID_3x3, State.GRID_4x4, State.GRID_GOLDEN], State.GRID);
  const timerType =
      condState([State.TIMER_3SEC, State.TIMER_10SEC], State.TIMER);
  const windowMaximizedState = condState([State.MAX_WND]);
  const windowPortraitState = condState([State.TALL]);
  const micMutedState = condState([State.MIC], Mode.VIDEO, true);
  const fpsType = condState([State.FPS_30, State.FPS_60], Mode.VIDEO, true);
  sendEvent(mode, {
    value: duration,
    [Ga4MetricDimension.EVENT_CATEGORY]: 'capture',
    [Ga4MetricDimension.EVENT_LABEL]: facing,
    [Ga4MetricDimension.MIRROR]: mirrorState,
    [Ga4MetricDimension.GRID]: gridType,
    [Ga4MetricDimension.TIMER]: timerType,
    [Ga4MetricDimension.MICROPHONE]: micMutedState,
    [Ga4MetricDimension.MAXIMIZED]: windowMaximizedState,
    [Ga4MetricDimension.TALL_ORIENTATION]: windowPortraitState,
    [Ga4MetricDimension.RESOLUTION]: resolution.toString(),
    [Ga4MetricDimension.FPS]: fpsType,
    [Ga4MetricDimension.INTENT_RESULT]: intentResult,
    [Ga4MetricDimension.SHUTTER_TYPE]: shutterType,
    [Ga4MetricDimension.IS_VIDEO_SNAPSHOT]: boolToIntString(isVideoSnapshot),
    [Ga4MetricDimension.EVER_PAUSED]: boolToIntString(everPaused),
    [Ga4MetricDimension.RECORD_TYPE]: String(recordType),
    [Ga4MetricDimension.GIF_RESULT]: String(gifResult),
    [Ga4MetricDimension.DURATION]: String(duration),
    [Ga4MetricDimension.RESOLUTION_LEVEL]: resolutionLevel,
    [Ga4MetricDimension.ASPECT_RATIO_SET]: String(aspectRatioSet),
    [Ga4MetricDimension.TIME_LAPSE_SPEED]: String(timeLapseSpeed),
    [Ga4MetricDimension.ZOOM_RATIO]: zoomRatio.toFixed(1),
  });

  void (async () => {
    const captureEvent: mojoType.CaptureEventParams = {
      mode: mojoTypeUtils.convertModeToMojo(mode),
      facing: mojoTypeUtils.convertFacingToMojo(facing),
      isMirrored: mirrorState === State.MIRROR,
      gridType: mojoTypeUtils.convertGridTypeToMojo(gridType),
      timerType: mojoTypeUtils.convertTimerTypeToMojo(timerType),
      shutterType: mojoTypeUtils.convertShutterTypeToMojo(shutterType),
      androidIntentResultType:
          mojoTypeUtils.convertIntentResultToMojo(intentResult),
      isWindowMaximized: windowMaximizedState === State.MAX_WND,
      isWindowPortrait: windowPortraitState === State.TALL,
      resolutionWidth: resolution.width,
      resolutionHeight: resolution.height,
      resolutionLevel:
          mojoTypeUtils.convertResolutionLevelToMojo(resolutionLevel),
      aspectRatioSet: mojoTypeUtils.convertAspectRatioSetToMojo(aspectRatioSet),
      captureDetails: null,
      zoomRatio,
    };
    if (mode === Mode.PHOTO || isVideoSnapshot) {
      captureEvent.captureDetails = {
        photoDetails: {
          isVideoSnapshot,
        },
      };
    } else if (mode === Mode.VIDEO) {
      const captureDetails = {
        videoDetails: {
          isMuted: micMutedState === State.MIC,
          fps: mojoTypeUtils.convertFpsTypeToMojo(fpsType),
          everPaused,
          duration,
          recordTypeDetails: {},
        },
      };

      let recordTypeDetails = null;
      if (recordType === RecordType.NORMAL_VIDEO) {
        recordTypeDetails = {normalVideoDetails: {}};
      } else if (recordType === RecordType.GIF) {
        recordTypeDetails = {
          gifVideoDetails: {
            gifResultType: mojoTypeUtils.convertGifResultTypeToMojo(gifResult),
          },
        };
      } else if (recordType === RecordType.TIME_LAPSE) {
        recordTypeDetails = {
          timelapseVideoDetails: {
            timelapseSpeed: Math.trunc(timeLapseSpeed),
          },
        };
      }
      assert(recordTypeDetails !== null);
      captureDetails.videoDetails.recordTypeDetails = recordTypeDetails;
      captureEvent.captureDetails = captureDetails;
    }
    (await getEventsSender()).sendCaptureEvent(captureEvent);
  })();
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
  const pageCount = perfInfo.pageCount ?? '';
  const pressure = assertExists(perfInfo.pressure);
  sendEvent(event, {
    value: duration,
    [Ga4MetricDimension.EVENT_CATEGORY]: 'perf',
    [Ga4MetricDimension.EVENT_LABEL]: facing,
    [Ga4MetricDimension.RESOLUTION]: `${resolution}`,
    [Ga4MetricDimension.DOC_PAGE_COUNT]: `${pageCount}`,
    [Ga4MetricDimension.PRESSURE]: `${pressure}`,
  });
  void (async () => {
    (await getEventsSender()).sendPerfEvent({
      eventType: mojoTypeUtils.convertPerfEventTypeToMojo(event),
      duration,
      facing: mojoTypeUtils.convertFacingToMojo(perfInfo.facing ?? null),
      resolutionWidth: perfInfo.resolution?.width ?? 0,
      resolutionHeight: perfInfo.resolution?.height ?? 0,
      pageCount: perfInfo.pageCount ?? 0,
      pressure: mojoTypeUtils.convertPressureToMojo(pressure),
    });
  })();
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
  sendEvent(mode, {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'intent',
    [Ga4MetricDimension.EVENT_LABEL]: result,
    [Ga4MetricDimension.INTENT_RESULT]: result,
    [Ga4MetricDimension.SHOULD_HANDLE_RESULT]:
        boolToIntString(shouldHandleResult),
    [Ga4MetricDimension.SHOULD_DOWN_SCALE]: boolToIntString(shouldDownScale),
    [Ga4MetricDimension.IS_SECURE]: boolToIntString(isSecure),
  });
  void (async () => {
    (await getEventsSender()).sendAndroidIntentEvent({
      mode: mojoTypeUtils.convertModeToMojo(mode),
      shouldHandleResult,
      shouldDownscale: shouldDownScale,
      isSecure,
    });
  })();
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
  sendEvent(type, {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'error',
    [Ga4MetricDimension.EVENT_LABEL]: level,
    [Ga4MetricDimension.ERROR_NAME]: errorName,
    [Ga4MetricDimension.FILENAME]: fileName,
    [Ga4MetricDimension.FUNC_NAME]: funcName,
    [Ga4MetricDimension.LINE_NO]: lineNo,
    [Ga4MetricDimension.COL_NO]: colNo,
  });
}

/**
 * Sends the barcode enabled event.
 */
export function sendBarcodeEnabledEvent(): void {
  sendEvent('enable', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'barcode',
  });
}

/**
 * Types of the decoded barcode content.
 */
export enum BarcodeContentType {
  TEXT = 'text',
  URL = 'url',
  WIFI = 'wifi',
}

interface BarcodeDetectedEventParam {
  contentType: BarcodeContentType;
}

/**
 * Sends the barcode detected event.
 */
export function sendBarcodeDetectedEvent(
    {contentType}: BarcodeDetectedEventParam, wifiSecurityType = ''): void {
  sendEvent('detect', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'barcode',
    [Ga4MetricDimension.EVENT_LABEL]: contentType,
    [Ga4MetricDimension.WIFI_SECURITY_TYPE]: wifiSecurityType,
  });

  void (async () => {
    (await getEventsSender()).sendBarcodeDetectedEvent({
      contentType: mojoTypeUtils.convertBarcodeContentTypeToMojo(contentType),
      wifiSecurityType:
          mojoTypeUtils.convertWifiSecurityTypeToMojo(wifiSecurityType),
    });
  })();
}

/**
 * Sends the open ptz panel event.
 */
export function sendOpenPTZPanelEvent(
    capabilities: {pan: boolean, tilt: boolean, zoom: boolean}): void {
  sendEvent('open-panel', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'ptz',
    [Ga4MetricDimension.SUPPORT_PAN]: boolToIntString(capabilities.pan),
    [Ga4MetricDimension.SUPPORT_TILT]: boolToIntString(capabilities.tilt),
    [Ga4MetricDimension.SUPPORT_ZOOM]: boolToIntString(capabilities.zoom),
  });
  void (async () => {
    (await getEventsSender()).sendOpenPTZPanelEvent({
      supportPan: capabilities.pan,
      supportTilt: capabilities.tilt,
      supportZoom: capabilities.zoom,
    });
  })();
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
  sendEvent(action, {
    value: fixCount,
    [Ga4MetricDimension.EVENT_CATEGORY]: 'doc-scan',
    [Ga4MetricDimension.DOC_FIX_TYPE]: String(fixType),
    [Ga4MetricDimension.DOC_PAGE_COUNT]: String(pageCount),
  });
  void (async () => {
    (await getEventsSender()).sendDocScanResultEvent({
      resultType: mojoTypeUtils.convertDocScanResultTypeToMojo(action),
      fixTypesMask: mojoTypeUtils.convertDocScanFixTypeToMojo(fixType),
      fixCount,
      pageCount,
    });
  })();
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
  sendEvent(action, {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'doc-scan',
  });
  void (async () => {
    (await getEventsSender()).sendDocScanActionEvent({
      actionType: mojoTypeUtils.convertDocScanActionTypeToMojo(action),
    });
  })();
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
  sendEvent(action, {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'low-storage',
  });

  void (async () => {
    (await getEventsSender()).sendLowStorageActionEvent({
      actionType: mojoTypeUtils.convertLowStorageActionTypeToMojo(action),
    });
  })();
}

function boolToIntString(b: boolean) {
  return b ? '1' : '0';
}

// The returned value reflects the logging consent option in OS settings.
async function checkCanSendMetrics(): Promise<boolean> {
  return !PRODUCTION ||
      await ChromeHelper.getInstance().isMetricsAndCrashReportingEnabled();
}

/**
 * Set of Top 20 Popular Camera Peripherals' Module ID from
 * go/usb-popularity-study. Since 4 cameras of Sonix have the same module ids,
 * they are aggregated to `Cam_Sonix`.
 */
export class PopularCamPeripheralSet {
  private readonly moduleIDSet: Set<string>;

  constructor() {
    this.moduleIDSet = new Set<string>([
      '046d:085b',  // C925e_Logitech
      '046d:0825',  // C270_Logitech
      '0c45:636b',  // Cam_Sonix
      '0c45:6366',  // VitadeAF_Microdia
      '046d:0843',  // C930e_Logitech
      '046d:082d',  // HDProC920_Logitech
      '046d:0892',  // C920HDPro_Logitech
      '046d:08e5',  // C920PROHD_Logitech
      '05a3:9331',  // Cam_ARC
      '046d:085e',  // BRIOUltraHD_Logitech
      '046d:085c',  // C922ProStream_Logitech
      '1b3f:2002',  // 808Camera9_Generalplus
      '1d6c:0103',  // NexiGoN60FHD_2MUVC
      '046d:082c',  // HDC615_Logitech
      '1778:d021',  // VZR_IPEVO
      '07ca:313a',  // LiveStreamer313_Sunplus
      '045e:0810',  // LifeCamHD3000_Microsoft
    ]);
  }

  has(moduleId: string): boolean {
    return this.moduleIDSet.has(moduleId);
  }

  /**
   * Returns the original `moduleId` if it exists in `moduleIDSet`. If not,
   * returns 'others'.
   */
  getMaskedId(moduleId: string): string {
    if (this.moduleIDSet.has(moduleId)) {
      return moduleId;
    }
    return 'others';
  }
}

const moduleIdSet = new PopularCamPeripheralSet();

/**
 * Sends camera opening event.
 *
 * @param moduleId Camera Module ID in the format of 8 digits hex string, such
 *     as abcd:1234.
 */
export function sendOpenCameraEvent(moduleId: string|null): void {
  const newModuleId =
      moduleId === null ? 'MIPI' : moduleIdSet.getMaskedId(moduleId);

  sendEvent('open-camera', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'open-camera',
    [Ga4MetricDimension.CAMERA_MODULE_ID]: newModuleId,
  });

  const params = {cameraModule: {}};
  if (moduleId === null) {
    params.cameraModule = {
      mipiCamera: {},
    };
  } else {
    params.cameraModule = {
      usbCamera: {id: moduleIdSet.has(moduleId) ? moduleId : null},
    };
  }
  void (async () => {
    (await getEventsSender()).sendOpenCameraEvent(params);
  })();
}

/**
 * Sends unsupported protocol event.
 */
export function sendUnsupportedProtocolEvent(): void {
  sendEvent('unsupportedProtocol', {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'barcode',
  });

  void (async () => {
    (await getEventsSender()).sendUnsupportedProtocolEvent();
  })();
}

/**
 * Updates the memory usage and session behavior value to untrusted_ga_helpers.
 *
 * @param updatedValue Updated memory usage dimensions value to be updated.
 */
export function updateMemoryUsageEventDimensions(
    updatedValue: MemoryUsageEventDimension): void {
  // No caller uses the returned promise.
  void (async () => {
    const gaHelper = await getGaHelper();
    await gaHelper.updateMemoryUsageEventDimensions(updatedValue);

    (await getEventsSender()).updateMemoryUsageEventParams({
      behaviorsMask: mojoTypeUtils.convertSessionBehaviorToMojo(
          updatedValue.sessionBehavior),
      memoryUsage: BigInt(updatedValue.memoryUsage),
    });
  })();
}

export enum OcrEventType {
  COPY_TEXT = 'copy-text',
  TEXT_DETECTED = 'text-detected',
}

interface OcrEventParams {
  eventType: OcrEventType;
  result: mojoType.OcrResult;
}

/**
 * Sends an OCR event.
 */
export function sendOcrEvent({eventType, result}: OcrEventParams): void {
  assert(result.lines.length > 0);
  const lineCount = result.lines.length;
  const wordCount =
      result.lines.reduce((acc, line) => acc + line.words.length, 0);
  const isPrimaryLanguage =
      getElementsWithMaxOccurrence(result.lines.map((line) => line.language))
          // Drop subtags from `navigator.language`. For example, 'en-US'
          // becomes 'en'.
          .includes(navigator.language.split('-')[0]);
  sendEvent(eventType, {
    [Ga4MetricDimension.EVENT_CATEGORY]: 'ocr',
    [Ga4MetricDimension.IS_PRIMARY_LANGUAGE]:
        boolToIntString(isPrimaryLanguage),
    [Ga4MetricDimension.LINE_COUNT]: String(lineCount),
    [Ga4MetricDimension.WORD_COUNT]: String(wordCount),
  });

  void (async () => {
    (await getEventsSender()).sendOcrEvent({
      eventType: mojoTypeUtils.convertOcrEventTypeToMojo(eventType),
      isPrimaryLanguage,
      lineCount,
      wordCount,
    });
  })();
}

function getElementsWithMaxOccurrence<T>(elements: T[]) {
  const map = new Map<T, number>();
  let elementsWithMaxOccurrence: T[] = [];
  let maxOccurrence = 0;
  for (const element of elements) {
    const occurrence = (map.get(element) ?? 0) + 1;
    if (maxOccurrence < occurrence) {
      maxOccurrence = occurrence;
      elementsWithMaxOccurrence = [element];
    } else if (occurrence === maxOccurrence) {
      elementsWithMaxOccurrence.push(element);
    }
    map.set(element, occurrence);
  }
  return elementsWithMaxOccurrence;
}
