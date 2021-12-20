// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {
  Mode,
  PerfEvent,
  PerfInformation,  // eslint-disable-line no-unused-vars
  ViewName,
} from './type.js';

/**
 * @enum {string}
 */
export const State = {
  CAMERA_CONFIGURING: 'camera-configuring',
  CAMERA_SWITCHING: 'camera-switching',
  CUSTOM_VIDEO_PARAMETERS: 'custom-video-parameters',
  ENABLE_FULL_SIZED_VIDEO_SNAPSHOT: 'enable-full-sized-video-snapshot',
  ENABLE_GIF_RECORDING: 'enable-gif-recording',
  ENABLE_MULTISTREAM_RECORDING: 'enable-multistream-recording',
  ENABLE_PTZ: 'enable-ptz',
  ENABLE_SCAN_BARCODE: 'enable-scan-barcode',
  EXPERT: 'expert',
  FPS_30: 'fps-30',
  FPS_60: 'fps-60',
  GRID_3x3: 'grid-3x3',
  GRID_4x4: 'grid-4x4',
  GRID_GOLDEN: 'grid-golden',
  GRID: 'grid',
  HAS_BACK_CAMERA: 'has-back-camera',
  HAS_EXTERNAL_SCREEN: 'has-external-screen',
  HAS_FRONT_CAMERA: 'has-front-camera',
  HAS_PAN_SUPPORT: 'has-pan-support',
  HAS_TILT_SUPPORT: 'has-tilt-support',
  HAS_ZOOM_SUPPORT: 'has-zoom-support',
  INTENT: 'intent',
  IS_NEW_FEATURE_TOAST_SHOWN: 'is-new-feature-toast-shown',
  MAX_WND: 'max-wnd',
  MIC: 'mic',
  MIRROR: 'mirror',
  MODE_SWITCHING: 'mode-switching',
  MULTI_CAMERA: 'multi-camera',
  MULTI_FPS: 'multi-fps',
  NO_RESOLUTION_SETTINGS: 'no-resolution-settings',
  PLAYING_RESULT_VIDEO: 'playing-result-video',
  PRINT_PERFORMANCE_LOGS: 'print-performance-logs',
  RECORD_TYPE_GIF: 'record-type-gif',
  RECORD_TYPE_NORMAL: 'record-type-normal',
  // Starts/Ends when start/stop event of MediaRecorder is triggered.
  RECORDING: 'recording',
  // Binds with paused state of MediaRecorder.
  RECORDING_PAUSED: 'recording-paused',
  // Controls appearance of paused/resumed UI.
  RECORDING_UI_PAUSED: 'recording-ui-paused',
  REVIEW_PHOTO_RESULT: 'review-photo-result',
  REVIEW_RESULT: 'review-result',
  REVIEW_VIDEO_RESULT: 'review-video-result',
  SAVE_METADATA: 'save-metadata',
  SCREEN_OFF_AUTO: 'screen-off-auto',
  SHOULD_HANDLE_INTENT_RESULT: 'should-handle-intent-result',
  SHOW_GIF_RECORDING_OPTION: 'show-gif-recording-option',
  SHOW_METADATA: 'show-metadata',
  SHOW_SCAN_MODE: 'show-scan-mode',
  SHUTTER_PROGRESSING: 'shutter-progressing',
  SNAPSHOTTING: 'snapshotting',
  STREAMING: 'streaming',
  SUSPEND: 'suspend',
  TABLET: 'tablet',
  TABLET_LANDSCAPE: 'tablet-landscape',
  TAB_NAVIGATION: 'tab-navigation',
  TAKING: 'taking',
  TALL: 'tall',
  TIMER_10SEC: 'timer-10s',
  TIMER_3SEC: 'timer-3s',
  TIMER: 'timer',
  USE_FAKE_CAMERA: 'use-fake-camera',
};

/**
 * @typedef {(!State|!Mode|!ViewName|!PerfEvent)}
 */
export let StateUnion;

const stateValues =
    new Set([State, Mode, ViewName, PerfEvent].flatMap(Object.values));

/**
 * Asserts input string is valid state.
 * @param {string} s
 * @return {!State}
 */
export function assertState(s) {
  assert(stateValues.has(s), `No such state: ${s}`);
  return /** @type {!State} */ (s);
}

/**
 * @typedef {function(boolean, !PerfInformation=): void}
 */
let StateObserver;  // eslint-disable-line no-unused-vars

/**
 * @type {!Map<!StateUnion, !Set<!StateObserver>>}
 */
const allObservers = new Map();

/**
 * Adds observer function to be called on any state change.
 * @param {!StateUnion} state State to be observed.
 * @param {!StateObserver} observer Observer function called with
 *     newly changed value.
 */
export function addObserver(state, observer) {
  let observers = allObservers.get(state);
  if (observers === undefined) {
    observers = new Set();
    allObservers.set(state, observers);
  }
  observers.add(observer);
}

/**
 * Adds one-time observer function to be called on any state change.
 * @param {!StateUnion} state State to be observed.
 * @param {!StateObserver} observer Observer function called with
 *     newly changed value.
 */
export function addOneTimeObserver(state, observer) {
  /** @type {StateObserver} */
  const wrappedObserver = (...args) => {
    observer(...args);
    removeObserver(state, wrappedObserver);
  };
  addObserver(state, wrappedObserver);
}

/**
 * Removes observer function to be called on state change.
 * @param {!StateUnion} state State to remove observer from.
 * @param {!StateObserver} observer Observer function to be removed.
 * @return {boolean} Whether the observer is in the set and is removed
 *     successfully or not.
 */
export function removeObserver(state, observer) {
  const observers = allObservers.get(state);
  if (observers === undefined) {
    return false;
  }
  return observers.delete(observer);
}

/**
 * Checks if the specified state exists.
 * @param {!StateUnion} state State to be checked.
 * @return {boolean} Whether the state exists.
 */
export function get(state) {
  return document.body.classList.contains(state);
}

/**
 * Sets the specified state on or off. Optionally, pass the information for
 * performance measurement.
 * @param {!StateUnion} state State to be set.
 * @param {boolean} val True to set the state on, false otherwise.
 * @param {!PerfInformation=} perfInfo Optional information of this state
 *     for performance measurement.
 */
export function set(state, val, perfInfo = {}) {
  const oldVal = get(state);
  if (oldVal === val) {
    return;
  }

  document.body.classList.toggle(state, val);
  const observers = allObservers.get(state) || [];
  observers.forEach((f) => f(val, perfInfo));
}
