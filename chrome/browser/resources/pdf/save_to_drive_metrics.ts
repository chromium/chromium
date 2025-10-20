// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import {SaveToDriveState} from './constants.js';
import {record, recordEnumeration, UserAction} from './metrics.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

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
 * Records when the Save to Drive bubble is shown in a certain state.
 */
function recordSaveToDriveBubbleState(bubbleState: SaveToDriveBubbleState) {
  recordEnumeration(
      'PDF.SaveToDrive.BubbleState', bubbleState, SaveToDriveBubbleState.COUNT);
}

/**
 * Records when a Save to Drive action is performed.
 */
function recordSaveToDriveSaveType(saveToDriveSaveType: SaveToDriveSaveType) {
  recordEnumeration(
      'PDF.SaveToDrive.SaveType', saveToDriveSaveType,
      SaveToDriveSaveType.COUNT);
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
    requestType: chrome.pdfViewerPrivate.SaveRequestType, hasEdits: boolean,
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
          hasEdits ? SaveToDriveSaveType.SAVE_ORIGINAL :
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
