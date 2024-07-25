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
import {AssertNever, CheckEnumValuesOverlap} from './type_utils.js';

export enum State {
  CAMERA_CONFIGURING = 'camera-configuring',
  // Disables resolution filter for video steams for tests that relies on all
  // resolutions being available.
  DISABLE_VIDEO_RESOLUTION_FILTER_FOR_TESTING =
      'disable-video-resolution-filter-for-testing',
  DOC_MODE_REVIEWING = 'doc-mode-reviewing',
  ENABLE_GIF_RECORDING = 'enable-gif-recording',
  ENABLE_PREVIEW_OCR = 'enable-preview-ocr',
  ENABLE_PTZ = 'enable-ptz',
  ENABLE_SCAN_BARCODE = 'enable-scan-barcode',
  ENABLE_SCAN_DOCUMENT = 'enable-scan-document',
  FPS_30 = 'fps-30',
  FPS_60 = 'fps-60',
  GRID = 'grid',
  /* eslint-disable @typescript-eslint/naming-convention */
  GRID_3x3 = 'grid-3x3',
  GRID_4x4 = 'grid-4x4',
  /* eslint-enable @typescript-eslint/naming-convention */
  GRID_GOLDEN = 'grid-golden',
  HAS_PAN_SUPPORT = 'has-pan-support',
  HAS_TILT_SUPPORT = 'has-tilt-support',
  HAS_ZOOM_SUPPORT = 'has-zoom-support',
  // Hides all toasts, nudges and tooltips for tests that relies on visual and
  // might be affected by these UIs appearing at incorrect timing.
  HIDE_FLOATING_UI_FOR_TESTING = 'hide-floating-ui-for-testing',
  INTENT = 'intent',
  KEYBOARD_NAVIGATION = 'keyboard-navigation',
  LID_CLOSED = 'lid_closed',
  MAX_WND = 'max-wnd',
  MIC = 'mic',
  MIRROR = 'mirror',
  MULTI_CAMERA = 'multi-camera',
  RECORD_TYPE_GIF = 'record-type-gif',
  RECORD_TYPE_NORMAL = 'record-type-normal',
  RECORD_TYPE_TIME_LAPSE = 'record-type-time-lapse',
  // Starts/Ends when start/stop event of MediaRecorder is triggered.
  RECORDING = 'recording',
  // Binds with paused state of MediaRecorder.
  RECORDING_PAUSED = 'recording-paused',
  // Controls appearance of paused/resumed UI.
  RECORDING_UI_PAUSED = 'recording-ui-paused',
  SHOULD_HANDLE_INTENT_RESULT = 'should-handle-intent-result',
  SNAPSHOTTING = 'snapshotting',
  STREAMING = 'streaming',
  SUPER_RES_ZOOM = 'super-res-zoom',
  SUSPEND = 'suspend',
  SW_PRIVACY_SWITCH_ON = 'sw-privacy-switch-on',
  TABLET = 'tablet',
  TABLET_LANDSCAPE = 'tablet-landscape',
  TAKING = 'taking',
  TALL = 'tall',
  TIMER = 'timer',
  TIMER_10SEC = 'timer-10s',
  TIMER_3SEC = 'timer-3s',
  TIMER_TICK = 'timer-tick',
  USE_FAKE_CAMERA = 'use-fake-camera',
}

// All the string enum types that can be accepted by |get| / |set|.
const stateEnums = [ExpertOption, Mode, PerfEvent, State, ViewName] as const;

// Note that this can't be inlined into StateTypes since this relies on
// https://github.com/microsoft/TypeScript/pull/26063
type MapEnumTypesToEnums<T> = {
  -readonly[K in keyof T]: T[K][keyof T[K]]
};
type StateTypes = MapEnumTypesToEnums<typeof stateEnums>;

// The union of all types accepted by |get| / |set|.
export type StateUnion = StateTypes[number];

const stateValues =
    new Set<StateUnion>(stateEnums.flatMap((s) => Object.values(s)));

/**
 * Asserts input string is valid state.
 */
export function assertState(s: string): StateUnion {
  // This is to workaround current TypeScript limitation on Set.has.
  // See https://github.com/microsoft/TypeScript/issues/26255
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  assert((stateValues as Set<string>).has(s), `No such state: ${s}`);
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
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
 * Adds observer function to be called when the state value is changed to true.
 * Returns the wrapped observer in case it needs to be removed later.
 *
 * @param state State to be observed.
 * @param observer Observer function called with newly changed value.
 */
export function addEnabledStateObserver(
    state: StateUnion, observer: StateObserver): StateObserver {
  const wrappedObserver: StateObserver = (val, perfInfo) => {
    if (val) {
      observer(val, perfInfo);
    }
  };
  addObserver(state, wrappedObserver);
  return wrappedObserver;
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
 * Gets if the specified state is on or off.
 *
 * @param state State to be checked.
 * @return Whether the state is on or off.
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

// This checks that all the enums in stateEnums doesn't overlap, so we don't
// accidentally have two different enum defining the same value.
type OverlappingStates = CheckEnumValuesOverlap<StateTypes>;

// This is exported here to avoid getting unused type warning from typescript.
//
// If you got compile error here, check type of OverlappingStates to see which
// values are duplicated between the stateEnums, and change one of those.
export type AssertStatesNotOverlapping = AssertNever<OverlappingStates>;
