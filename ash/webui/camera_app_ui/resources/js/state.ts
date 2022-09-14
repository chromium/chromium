// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {ExpertOption} from './expert.js';
import {
  Mode,
  PerfEvent,
  PerfInformation,
  ViewName,
} from './type.js';

export enum State {
  CAMERA_CONFIGURING = 'camera-configuring',
  CAMERA_SWITCHING = 'camera-switching',
  DOC_MODE_REVIEWING = 'doc-mode-reviewing',
  ENABLE_GIF_RECORDING = 'enable-gif-recording',
  ENABLE_PTZ = 'enable-ptz',
  ENABLE_SCAN_BARCODE = 'enable-scan-barcode',
  FPS_30 = 'fps-30',
  FPS_60 = 'fps-60',
  /* eslint-disable @typescript-eslint/naming-convention */
  GRID_3x3 = 'grid-3x3',
  GRID_4x4 = 'grid-4x4',
  /* eslint-enable @typescript-eslint/naming-convention */
  GRID_GOLDEN = 'grid-golden',
  GRID = 'grid',
  HAS_BACK_CAMERA = 'has-back-camera',
  HAS_FRONT_CAMERA = 'has-front-camera',
  HAS_PAN_SUPPORT = 'has-pan-support',
  HAS_TILT_SUPPORT = 'has-tilt-support',
  HAS_ZOOM_SUPPORT = 'has-zoom-support',
  INTENT = 'intent',
  KEYBOARD_NAVIGATION = 'keyboard-navigation',
  MAX_WND = 'max-wnd',
  MIC = 'mic',
  MIRROR = 'mirror',
  MODE_SWITCHING = 'mode-switching',
  MULTI_CAMERA = 'multi-camera',
  PLAYING_RESULT_VIDEO = 'playing-result-video',
  RECORD_TYPE_GIF = 'record-type-gif',
  RECORD_TYPE_NORMAL = 'record-type-normal',
  // Starts/Ends when start/stop event of MediaRecorder is triggered.
  RECORDING = 'recording',
  // Binds with paused state of MediaRecorder.
  RECORDING_PAUSED = 'recording-paused',
  // Controls appearance of paused/resumed UI.
  RECORDING_UI_PAUSED = 'recording-ui-paused',
  SHOULD_HANDLE_INTENT_RESULT = 'should-handle-intent-result',
  SHOW_GIF_RECORDING_OPTION = 'show-gif-recording-option',
  SHUTTER_PROGRESSING = 'shutter-progressing',
  SNAPSHOTTING = 'snapshotting',
  STREAMING = 'streaming',
  SUSPEND = 'suspend',
  TABLET = 'tablet',
  TABLET_LANDSCAPE = 'tablet-landscape',
  TAKING = 'taking',
  TALL = 'tall',
  TIMER_10SEC = 'timer-10s',
  TIMER_3SEC = 'timer-3s',
  TIMER = 'timer',
  USE_FAKE_CAMERA = 'use-fake-camera',
}

export type StateUnion = ExpertOption|Mode|PerfEvent|State|ViewName;

const stateValues = new Set<StateUnion>([
  State,
  Mode,
  ViewName,
  PerfEvent,
  ExpertOption,
].flatMap((s) => Object.values(s)));

/**
 * Asserts input string is valid state.
 */
export function assertState(s: string): StateUnion {
  assert((stateValues as Set<string>).has(s), `No such state: ${s}`);
  return s as StateUnion;
}

export type StateObserver = (val: boolean, perfInfo: PerfInformation) => void;

const allObservers = new Map<StateUnion, Set<StateObserver>>();

/**
 * Adds observer function to be called on any state change.
 *
 * @param state State to be observed.
 * @param observer Observer function called with newly changed value.
 */
export function addObserver(state: StateUnion, observer: StateObserver): void {
  let observers = allObservers.get(state);
  if (observers === undefined) {
    observers = new Set();
    allObservers.set(state, observers);
  }
  observers.add(observer);
}

/**
 * Adds one-time observer function to be called on any state change.
 *
 * @param state State to be observed.
 * @param observer Observer function called with newly changed value.
 */
export function addOneTimeObserver(
    state: StateUnion, observer: StateObserver): void {
  const wrappedObserver: StateObserver = (...args) => {
    observer(...args);
    removeObserver(state, wrappedObserver);
  };
  addObserver(state, wrappedObserver);
}

/**
 * Removes observer function to be called on state change.
 *
 * @param state State to remove observer from.
 * @param observer Observer function to be removed.
 * @return Whether the observer is in the set and is removed successfully or
 *     not.
 */
export function removeObserver(
    state: StateUnion, observer: StateObserver): boolean {
  const observers = allObservers.get(state);
  if (observers === undefined) {
    return false;
  }
  return observers.delete(observer);
}

/**
 * Checks if the specified state exists.
 *
 * @param state State to be checked.
 * @return Whether the state exists.
 */
export function get(state: StateUnion): boolean {
  return document.body.classList.contains(state);
}

/**
 * Sets the specified state on or off. Optionally, pass the information for
 * performance measurement.
 *
 * @param state State to be set.
 * @param val True to set the state on, false otherwise.
 * @param perfInfo Optional information of this state for performance
 *     measurement.
 */
export function set(
    state: StateUnion, val: boolean, perfInfo: PerfInformation = {}): void {
  const oldVal = get(state);
  if (oldVal === val) {
    return;
  }

  document.body.classList.toggle(state, val);
  const observers = allObservers.get(state) ?? [];
  for (const observer of observers) {
    observer(val, perfInfo);
  }
}
