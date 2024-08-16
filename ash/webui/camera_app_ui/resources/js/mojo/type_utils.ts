// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
import {SessionBehavior} from '../memory_usage.js';
import {
  BarcodeContentType,
  DocScanActionType,
  DocScanFixType,
  DocScanResultActionType,
  GifResultType,
  IntentResultType,
  LaunchType,
  LowStorageActionType,
  OcrEventType,
  RecordType,
  ShutterType,
} from '../metrics.js';
import {SupportedWifiSecurityType} from '../scanner_chip.js';
import {State} from '../state.js';
import {
  AspectRatioSet,
  Facing,
  Mode,
  PerfEvent,
  PhotoResolutionLevel,
  Pressure,
  VideoResolutionLevel,
} from '../type.js';

import * as mojoType from './type.js';

/**
 * Converts the launch type to the mojo enum to be used in metrics.
 */
export function convertLaunchTypeToMojo(launchType: LaunchType):
    mojoType.LaunchType {
  switch (launchType) {
    case LaunchType.DEFAULT:
      return mojoType.LaunchType.kDefault;
    default:
      assertNotReached();
  }
}

/**
 * Converts the mode to the mojo enum to be used in metrics.
 */
export function convertModeToMojo(mode: string): mojoType.Mode {
  switch (mode) {
    case Mode.PHOTO:
      return mojoType.Mode.kPhoto;
    case Mode.VIDEO:
      return mojoType.Mode.kVideo;
    case Mode.SCAN:
      return mojoType.Mode.kScan;
    case Mode.PORTRAIT:
      return mojoType.Mode.kPortrait;
    default:
      assertNotReached();
  }
}

/**
 * Converts the camera facing to the mojo enum to be used in metrics.
 */
export function convertFacingToMojo(facing: Facing|null): mojoType.Facing {
  switch (facing) {
    case Facing.USER:
      return mojoType.Facing.kUser;
    case Facing.ENVIRONMENT:
      return mojoType.Facing.kEnvironment;
    case Facing.EXTERNAL:
      return mojoType.Facing.kExternal;
    default:
      return mojoType.Facing.kUnknown;
  }
}

/**
 * Converts the CPU pressure to the mojo enum to be used in metrics.
 */
export function convertPressureToMojo(pressure: Pressure): mojoType.Pressure {
  switch (pressure) {
    case Pressure.NOMINAL:
      return mojoType.Pressure.kNominal;
    case Pressure.FAIR:
      return mojoType.Pressure.kFair;
    case Pressure.SERIOUS:
      return mojoType.Pressure.kSerious;
    case Pressure.CRITICAL:
      return mojoType.Pressure.kCritical;
    default:
      assertNotReached();
  }
}

/**
 * Converts the grid type to the mojo enum to be used in metrics.
 */
export function convertGridTypeToMojo(gridType: string): mojoType.GridType {
  switch (gridType) {
    case State.GRID_3x3:
      return mojoType.GridType.k3X3;
    case State.GRID_4x4:
      return mojoType.GridType.k4X4;
    case State.GRID_GOLDEN:
      return mojoType.GridType.kGolden;
    default:
      return mojoType.GridType.kNone;
  }
}

/**
 * Converts the timer type to the mojo enum to be used in metrics.
 */
export function convertTimerTypeToMojo(timerType: string): mojoType.TimerType {
  switch (timerType) {
    case State.TIMER_3SEC:
      return mojoType.TimerType.k3Seconds;
    case State.TIMER_10SEC:
      return mojoType.TimerType.k10Seconds;
    default:
      return mojoType.TimerType.kNone;
  }
}

/**
 * Converts the shutter type to the mojo enum to be used in metrics.
 */
export function convertShutterTypeToMojo(shutterType: ShutterType):
    mojoType.ShutterType {
  switch (shutterType) {
    case ShutterType.KEYBOARD:
      return mojoType.ShutterType.kKeyboard;
    case ShutterType.MOUSE:
      return mojoType.ShutterType.kMouse;
    case ShutterType.TOUCH:
      return mojoType.ShutterType.kTouch;
    case ShutterType.VOLUME_KEY:
      return mojoType.ShutterType.kVolumeKey;
    default:
      return mojoType.ShutterType.kUnknown;
  }
}

/**
 * Converts the Android intent result type to the mojo enum to be used in
 * metrics.
 */
export function convertIntentResultToMojo(intentResult: IntentResultType):
    mojoType.AndroidIntentResultType {
  switch (intentResult) {
    case IntentResultType.CONFIRMED:
      return mojoType.AndroidIntentResultType.kConfirmed;
    case IntentResultType.CANCELED:
      return mojoType.AndroidIntentResultType.kCanceled;
    default:
      return mojoType.AndroidIntentResultType.kNotIntent;
  }
}

/**
 * Converts the resolution level to the mojo enum to be used in metrics.
 */
export function convertResolutionLevelToMojo(
    resolutionLevel: PhotoResolutionLevel|
    VideoResolutionLevel): mojoType.ResolutionLevel {
  switch (resolutionLevel) {
    case PhotoResolutionLevel.FULL:
    case VideoResolutionLevel.FULL:
      return mojoType.ResolutionLevel.kFull;
    case PhotoResolutionLevel.MEDIUM:
    case VideoResolutionLevel.MEDIUM:
      return mojoType.ResolutionLevel.kMedium;
    case VideoResolutionLevel.FOUR_K:
      return mojoType.ResolutionLevel.k4K;
    case VideoResolutionLevel.QUAD_HD:
      return mojoType.ResolutionLevel.kQuadHD;
    case VideoResolutionLevel.FULL_HD:
      return mojoType.ResolutionLevel.kFullHD;
    case VideoResolutionLevel.HD:
      return mojoType.ResolutionLevel.kHD;
    case VideoResolutionLevel.THREE_SIXTY_P:
      return mojoType.ResolutionLevel.k360P;
    default:
      return mojoType.ResolutionLevel.kUnknown;
  }
}

/**
 * Converts the aspect ratio set to the mojo enum to be used in metrics.
 */
export function convertAspectRatioSetToMojo(aspectRatioSet: AspectRatioSet):
    mojoType.AspectRatioSet {
  switch (aspectRatioSet) {
    case AspectRatioSet.RATIO_4_3:
      return mojoType.AspectRatioSet.k4To3;
    case AspectRatioSet.RATIO_16_9:
      return mojoType.AspectRatioSet.k16To9;
    case AspectRatioSet.RATIO_SQUARE:
      return mojoType.AspectRatioSet.kSquare;
    default:
      return mojoType.AspectRatioSet.kOthers;
  }
}

/**
 * Converts the GIF result type to the mojo enum to be used in metrics.
 */
export function convertGifResultTypeToMojo(gifResultType: GifResultType):
    mojoType.GifResultType {
  switch (gifResultType) {
    case GifResultType.RETAKE:
      return mojoType.GifResultType.kRetake;
    case GifResultType.SAVE:
      return mojoType.GifResultType.kSave;
    case GifResultType.SHARE:
      return mojoType.GifResultType.kShare;
    default:
      return mojoType.GifResultType.kNotGif;
  }
}

/**
 * Converts the video FPS type to the integer to be used in metrics.
 */
export function convertFpsTypeToMojo(fpsType: string): number {
  switch (fpsType) {
    case State.FPS_30:
      return 30;
    case State.FPS_60:
      return 60;
    default:
      return 0;
  }
}

/**
 * Converts the record type to the mojo enum to be used in metrics.
 */
export function convertRecordTypeToMojo(recordType: RecordType):
    mojoType.RecordType {
  switch (recordType) {
    case RecordType.NORMAL_VIDEO:
      return mojoType.RecordType.kNormal;
    case RecordType.GIF:
      return mojoType.RecordType.kGif;
    case RecordType.TIME_LAPSE:
      return mojoType.RecordType.kTimelapse;
    default:
      return mojoType.RecordType.kNotRecording;
  }
}

/**
 * Converts the document scanning action type to the mojo enum to be used in
 * metrics.
 */
export function convertDocScanActionTypeToMojo(actionType: DocScanActionType):
    mojoType.DocScanActionType {
  switch (actionType) {
    case DocScanActionType.ADD_PAGE:
      return mojoType.DocScanActionType.kAddPage;
    case DocScanActionType.DELETE_PAGE:
      return mojoType.DocScanActionType.kDeletePage;
    case DocScanActionType.FIX:
      return mojoType.DocScanActionType.kFix;
    default:
      assertNotReached();
  }
}

/**
 * Converts the document scanning result type to the mojo enum to be used in
 * metrics.
 */
export function convertDocScanResultTypeToMojo(
    resultType: DocScanResultActionType): mojoType.DocScanResultType {
  switch (resultType) {
    case DocScanResultActionType.CANCEL:
      return mojoType.DocScanResultType.kCancel;
    case DocScanResultActionType.SAVE_AS_PDF:
      return mojoType.DocScanResultType.kSaveAsPdf;
    case DocScanResultActionType.SAVE_AS_PHOTO:
      return mojoType.DocScanResultType.kSaveAsPhoto;
    case DocScanResultActionType.SHARE:
      return mojoType.DocScanResultType.kShare;
    default:
      assertNotReached();
  }
}

/**
 * Converts the document scanning fix type to the integer aligned with the
 * definition in the mojo enum that can be used in metrics.
 */
export function convertDocScanFixTypeToMojo(fixType: number): number {
  let mojoFixTypes = 0;
  if ((fixType & DocScanFixType.CORNER) !== 0) {
    mojoFixTypes |= mojoType.DocScanFixType.kCorner;
  }
  if ((fixType & DocScanFixType.ROTATION) !== 0) {
    mojoFixTypes |= mojoType.DocScanFixType.kRotation;
  }
  return mojoFixTypes;
}

/**
 * Converts the low storage action type to the mojo enum to be used in metrics.
 */
export function convertLowStorageActionTypeToMojo(
    actionType: LowStorageActionType): mojoType.LowStorageActionType {
  switch (actionType) {
    case LowStorageActionType.MANAGE_STORAGE_AUTO_STOP:
      return mojoType.LowStorageActionType.kManageStorageAutoStop;
    case LowStorageActionType.MANAGE_STORAGE_CANNOT_START:
      return mojoType.LowStorageActionType.kManageStorageCannotStart;
    case LowStorageActionType.SHOW_AUTO_STOP_DIALOG:
      return mojoType.LowStorageActionType.kShowAutoStopDialog;
    case LowStorageActionType.SHOW_CANNOT_START_DIALOG:
      return mojoType.LowStorageActionType.kShowCannotStartDialog;
    case LowStorageActionType.SHOW_WARNING_MSG:
      return mojoType.LowStorageActionType.kShowWarningMessage;
    default:
      assertNotReached();
  }
}

/**
 * Converts the barcode content type to the mojo enum to be used in metrics.
 */
export function convertBarcodeContentTypeToMojo(
    contentType: BarcodeContentType): mojoType.BarcodeContentType {
  switch (contentType) {
    case BarcodeContentType.TEXT:
      return mojoType.BarcodeContentType.kText;
    case BarcodeContentType.URL:
      return mojoType.BarcodeContentType.kUrl;
    case BarcodeContentType.WIFI:
      return mojoType.BarcodeContentType.kWiFi;
    default:
      assertNotReached();
  }
}

/**
 * Converts the Wi-Fi security type to the mojo enum to be used in metrics.
 */
export function convertWifiSecurityTypeToMojo(securityType: string):
    mojoType.WifiSecurityType {
  switch (securityType) {
    case SupportedWifiSecurityType.WEP:
      return mojoType.WifiSecurityType.kWep;
    case SupportedWifiSecurityType.WPA:
      return mojoType.WifiSecurityType.kWpa;
    case SupportedWifiSecurityType.EAP:
      return mojoType.WifiSecurityType.kEap;
    default:
      return mojoType.WifiSecurityType.kNone;
  }
}

/**
 * Converts the perf event type to the mojo enum to be used in metrics.
 */
export function convertPerfEventTypeToMojo(perfEventType: PerfEvent):
    mojoType.PerfEventType {
  switch (perfEventType) {
    case PerfEvent.CAMERA_SWITCHING:
      return mojoType.PerfEventType.kCameraSwitching;
    case PerfEvent.DOCUMENT_CAPTURE_POST_PROCESSING:
      return mojoType.PerfEventType.kDocumentCapturePostProcessing;
    case PerfEvent.DOCUMENT_PDF_SAVING:
      return mojoType.PerfEventType.kDocumentPdfSaving;
    case PerfEvent.GIF_CAPTURE_POST_PROCESSING:
      return mojoType.PerfEventType.kGifCapturePostProcessing;
    case PerfEvent.GIF_CAPTURE_SAVING:
      return mojoType.PerfEventType.kGifCaptureSaving;
    case PerfEvent.LAUNCHING_FROM_LAUNCH_APP_COLD:
      return mojoType.PerfEventType.kLaunchingFromLaunchAppCold;
    case PerfEvent.LAUNCHING_FROM_LAUNCH_APP_WARM:
      return mojoType.PerfEventType.kLaunchingFromLaunchAppWarm;
    case PerfEvent.LAUNCHING_FROM_WINDOW_CREATION:
      return mojoType.PerfEventType.kLaunchingFromWindowCreation;
    case PerfEvent.MODE_SWITCHING:
      return mojoType.PerfEventType.kModeSwitching;
    case PerfEvent.OCR_SCANNING:
      return mojoType.PerfEventType.kOcrScanning;
    case PerfEvent.PHOTO_CAPTURE_POST_PROCESSING_SAVING:
      return mojoType.PerfEventType.kPhotoCapturePostProcessingSaving;
    case PerfEvent.PHOTO_CAPTURE_SHUTTER:
      return mojoType.PerfEventType.kPhotoCaptureShutter;
    case PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING_SAVING:
      return mojoType.PerfEventType.kPortraitModeCapturePostProcessingSaving;
    case PerfEvent.SNAPSHOT_TAKING:
      return mojoType.PerfEventType.kSnapshotTaking;
    case PerfEvent.TIME_LAPSE_CAPTURE_POST_PROCESSING_SAVING:
      return mojoType.PerfEventType.kTimelapseCapturePostProcessingSaving;
    case PerfEvent.VIDEO_CAPTURE_POST_PROCESSING_SAVING:
      return mojoType.PerfEventType.kVideoCapturePostProcessingSaving;
    default:
      assertNotReached();
  }
}

/**
 * Converts the session behavior to the integer aligned with the definition in
 * the mojo enum that can be used in metrics.
 */
export function convertSessionBehaviorToMojo(sessionBehavior: number): number {
  let mojoBehaviors = 0;
  if ((sessionBehavior & SessionBehavior.TAKE_NORMAL_PHOTO) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kTakeNormalPhoto;
  }
  if ((sessionBehavior & SessionBehavior.TAKE_PORTRAIT_PHOTO) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kTakePortraitPhoto;
  }
  if ((sessionBehavior & SessionBehavior.SCAN_BARCODE) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kScanBarcode;
  }
  if ((sessionBehavior & SessionBehavior.SCAN_DOCUMENT) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kScanDocument;
  }
  if ((sessionBehavior & SessionBehavior.RECORD_NORMAL_VIDEO) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kRecordNormalVideo;
  }
  if ((sessionBehavior & SessionBehavior.RECORD_GIF_VIDEO) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kRecordGifVideo;
  }
  if ((sessionBehavior & SessionBehavior.RECORD_TIME_LAPSE_VIDEO) !== 0) {
    mojoBehaviors |= mojoType.UserBehavior.kRecordTimelapseVideo;
  }
  return mojoBehaviors;
}

/**
 * Converts the OCR event type to the mojo enum to be used in metrics.
 */
export function convertOcrEventTypeToMojo(ocrEventType: OcrEventType):
    mojoType.OcrEventType {
  switch (ocrEventType) {
    case OcrEventType.COPY_TEXT:
      return mojoType.OcrEventType.kCopyText;
    case OcrEventType.TEXT_DETECTED:
      return mojoType.OcrEventType.kTextDetected;
    default:
      assertNotReached();
  }
}
