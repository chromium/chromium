// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
import {
  DocScanActionType,
  DocScanFixType,
  DocScanResultActionType,
  GifResultType,
  IntentResultType,
  LaunchType,
  RecordType,
  ShutterType,
} from '../metrics.js';
import {State} from '../state.js';
import {
  AspectRatioSet,
  Facing,
  Mode,
  PhotoResolutionLevel,
  VideoResolutionLevel,
} from '../type.js';

import * as mojoType from './type.js';

/**
 * Converts the launch type to the mojo enum to be used in metrics.
 */
export function convertLaunchTypeToMojo(launchType: LaunchType):
    mojoType.LaunchType {
  switch (launchType) {
    case LaunchType.ASSISTANT:
      return mojoType.LaunchType.kAssistant;
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
export function convertFacingToMojo(facing: Facing): mojoType.Facing {
  switch (facing) {
    case Facing.USER:
      return mojoType.Facing.kUser;
    case Facing.ENVIRONMENT:
      return mojoType.Facing.kEnvironment;
    case Facing.EXTERNAL:
      return mojoType.Facing.kExternal;
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
    case ShutterType.ASSISTANT:
      return mojoType.ShutterType.kAssistant;
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
