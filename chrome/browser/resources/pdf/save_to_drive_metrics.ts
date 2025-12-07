// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import {SaveToDriveBubbleRequestType, SaveToDriveState} from './constants.js';
import {record, recordEnumeration, UserAction} from './metrics.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

/**
 * `SaveToDriveBubbleAction` is used for metrics. Entries should not be
 * renumbered, removed or reused.
 */
export enum SaveToDriveBubbleAction {
  // LINT.IfChange(PDFSaveToDriveBubbleAction)
  ACTION = 0,
  CANCEL_UPLOAD = 1,
  MANAGE_STORAGE = 2,
  OPEN_IN_DRIVE = 3,
  RETRY = 4,
  CLOSE = 5,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFSaveToDriveBubbleAction)

  // Must be the last one.
  COUNT = 6,
}

/**
 * `SaveToDriveBubbleState` is used for metrics. Entries should not be
 * renumbered, removed or reused.
 */
export enum SaveToDriveBubbleState {
  // LINT.IfChange(PDFSaveToDriveBubbleState)
  SHOW_BUBBLE = 0,
  SHOW_BUBBLE_UPLOADING_STATE = 1,
  SHOW_BUBBLE_SUCCESS_STATE = 2,
  SHOW_BUBBLE_CONNECTION_ERROR_STATE = 3,
  SHOW_BUBBLE_STORAGE_FULL_ERROR_STATE = 4,
  SHOW_BUBBLE_SESSION_TIMEOUT_ERROR_STATE = 5,
  SHOW_BUBBLE_UNKNOWN_ERROR_STATE = 6,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFSaveToDriveBubbleState)

  // Must be the last one.
  COUNT = 7,
}

/**
 * `SaveToDriveSaveType` is used for metrics. Entries should not be renumbered,
 * removed or reused.
 */
export enum SaveToDriveSaveType {
  // LINT.IfChange(PDFSaveToDriveSaveType)
  SAVE = 0,
  SAVE_ORIGINAL_ONLY = 1,
  SAVE_ORIGINAL = 2,
  SAVE_EDITED = 3,
  SAVE_WITH_ANNOTATION = 4,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFSaveToDriveSaveType)

  // Must be the last one.
  COUNT = 5,
}

/**
 * Records when a Save to Drive bubble action is performed.
 */
function recordSaveToDriveBubbleAction(action: SaveToDriveBubbleAction) {
  recordEnumeration(
      'PDF.SaveToDrive.BubbleAction', action, SaveToDriveBubbleAction.COUNT);
}

/**
 * Records when the Save to Drive bubble is shown in a certain state.
 */
function recordSaveToDriveBubbleState(bubbleState: SaveToDriveBubbleState) {
  recordEnumeration(
      'PDF.SaveToDrive.BubbleState', bubbleState, SaveToDriveBubbleState.COUNT);
}

/**
 * Records when a Save to Drive retry action is performed.
 */
function recordSaveToDriveRetrySaveType(
    retrySaveType: SaveToDriveSaveType) {
  recordEnumeration(
      'PDF.SaveToDrive.RetrySaveType', retrySaveType,
      SaveToDriveSaveType.COUNT);
}

/**
 * Records when a Save to Drive action is performed.
 */
function recordSaveToDriveSaveType(saveToDriveSaveType: SaveToDriveSaveType) {
  recordEnumeration(
      'PDF.SaveToDrive.SaveType', saveToDriveSaveType,
      SaveToDriveSaveType.COUNT);
}

export function recordSaveToDriveBubbleActionMetrics(
    requestType: SaveToDriveBubbleRequestType) {
  recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.ACTION);
  switch (requestType) {
    case SaveToDriveBubbleRequestType.CANCEL_UPLOAD:
      recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.CANCEL_UPLOAD);
      break;
    case SaveToDriveBubbleRequestType.MANAGE_STORAGE:
      recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.MANAGE_STORAGE);
      break;
    case SaveToDriveBubbleRequestType.OPEN_IN_DRIVE:
      recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.OPEN_IN_DRIVE);
      break;
    case SaveToDriveBubbleRequestType.RETRY:
      recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.RETRY);
      break;
    case SaveToDriveBubbleRequestType.DIALOG_CLOSED:
      recordSaveToDriveBubbleAction(SaveToDriveBubbleAction.CLOSE);
      break;
    default:
      assertNotReached('Unknown save to Drive bubble action: ' + requestType);
  }
}

export function recordSaveToDriveBubbleRetryMetrics(
    requestType: SaveRequestType, hasCommittedEdits: boolean) {
  // The general retry action is recorded in
  // `recordSaveToDriveBubbleActionMetrics()`.
  switch (requestType) {
    case SaveRequestType.ANNOTATION:
      recordSaveToDriveRetrySaveType(
          SaveToDriveSaveType.SAVE_WITH_ANNOTATION);
      break;
    case SaveRequestType.ORIGINAL:
      recordSaveToDriveRetrySaveType(
          hasCommittedEdits ? SaveToDriveSaveType.SAVE_ORIGINAL :
                              SaveToDriveSaveType.SAVE_ORIGINAL_ONLY);
      break;
    case SaveRequestType.EDITED:
      recordSaveToDriveRetrySaveType(SaveToDriveSaveType.SAVE_EDITED);
      break;
    default:
      assertNotReached(
          'Unknown save request type for Save to Drive: ' + requestType);
  }
}

export function recordShowSaveToDriveBubbleMetrics(
    saveToDriveState: SaveToDriveState) {
  recordSaveToDriveBubbleState(SaveToDriveBubbleState.SHOW_BUBBLE);
  switch (saveToDriveState) {
    case SaveToDriveState.UPLOADING:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_UPLOADING_STATE);
      break;
    case SaveToDriveState.SUCCESS:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_SUCCESS_STATE);
      break;
    case SaveToDriveState.CONNECTION_ERROR:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_CONNECTION_ERROR_STATE);
      break;
    case SaveToDriveState.STORAGE_FULL_ERROR:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_STORAGE_FULL_ERROR_STATE);
      break;
    case SaveToDriveState.SESSION_TIMEOUT_ERROR:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_SESSION_TIMEOUT_ERROR_STATE);
      break;
    case SaveToDriveState.UNKNOWN_ERROR:
      recordSaveToDriveBubbleState(
          SaveToDriveBubbleState.SHOW_BUBBLE_UNKNOWN_ERROR_STATE);
      break;
    default:
      assertNotReached(
          'Unknown state for showing save to Drive bubble: ' +
          saveToDriveState);
  }
}

export function recordSaveToDriveMetrics(
    requestType: SaveRequestType, hasCommittedEdits: boolean,
    pdfInk2Enabled: boolean) {
  recordSaveToDriveSaveType(SaveToDriveSaveType.SAVE);
  switch (requestType) {
    case SaveRequestType.ANNOTATION:
      if (pdfInk2Enabled) {
        record(UserAction.SAVE_WITH_INK2_ANNOTATION);
      }
      recordSaveToDriveSaveType(SaveToDriveSaveType.SAVE_WITH_ANNOTATION);
      break;
    case SaveRequestType.ORIGINAL:
      recordSaveToDriveSaveType(
          hasCommittedEdits ? SaveToDriveSaveType.SAVE_ORIGINAL :
                              SaveToDriveSaveType.SAVE_ORIGINAL_ONLY);
      break;
    case SaveRequestType.EDITED:
      recordSaveToDriveSaveType(SaveToDriveSaveType.SAVE_EDITED);
      break;
    default:
      assertNotReached(
          'Unknown save request type for Save to Drive: ' + requestType);
  }
}
