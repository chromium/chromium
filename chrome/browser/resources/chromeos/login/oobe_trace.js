// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OOBE Tracing Utilities
 *
 * This file contain utilities for measuring OOBE's frontend load timings. When
 * this file is first imported, it assumes the existence of an object in the
 * global object (window) that contains the timestamp  of when OOBE started.
 * This value ('window.oobeInitializationBeginTimestamp'), is set by another
 * script (oobe_trace_start.js) that runs as the first thing in OOBE.
 *
 * For now, the results are written into 'window' as 'oobeTraceLogs'. In the
 * future, these values will be available directly from the OOBE debugger and
 * will be written into an UMA metric.
 */

import {assert} from '//resources/ash/common/assert.js';
assert(window.oobeInitializationBeginTimestamp);

export const TraceEvent = {
  FIRST_LINE_AFTER_IMPORTS: 'FIRST_LINE_AFTER_IMPORTS',
  COMMON_SCREENS_ADDED: 'COMMON_SCREENS_ADDED',
  OOBE_SCREENS_ADDED: 'OOBE_SCREENS_ADDED',
  LOGIN_SCREENS_ADDED: 'LOGIN_SCREENS_ADDED',
  DOM_CONTENT_LOADED: 'DOM_CONTENT_LOADED',
  OOBE_INITIALIZED: 'OOBE_INITIALIZED',
  FIRST_SCREEN_SHOWN: 'FIRST_SCREEN_SHOWN',
  FIRST_OOBE_LOTTIE_INITIALIZED: 'FIRST_OOBE_LOTTIE_INITIALIZED',
  LAST_OOBE_LOTTIE_INITIALIZED: 'LAST_OOBE_LOTTIE_INITIALIZED',
  WELCOME_ANIMATION_PLAYING: 'WELCOME_ANIMATION_PLAYING',
};

// Event log containing a TraceEvent and a timestamp.
const eventLogs = [];
window.oobeTraceLogs = eventLogs;


function createEventEntry(traceEvent) {
  assert(traceEvent in TraceEvent);
  const currentTimestamp = new Date();
  const delta = currentTimestamp - window.oobeInitializationBeginTimestamp;
  const eventEntry = {};
  eventEntry[traceEvent] = delta;
  return eventEntry;
}

let firstScreenShownEventLogged = false;
let welcomeAnimationPlayEventLogged = false;
let firstOobeLottieEventLogged = false;

export function traceExecution(traceEvent) {
  const eventEntry = createEventEntry(traceEvent);
  eventLogs.push(eventEntry);
}

export function traceFirstScreenShown() {
  if (firstScreenShownEventLogged) {
    return;
  }
  firstScreenShownEventLogged = true;
  traceExecution(TraceEvent.FIRST_SCREEN_SHOWN);
}

export function traceWelcomeAnimationPlay() {
  if (welcomeAnimationPlayEventLogged) {
    return;
  }
  welcomeAnimationPlayEventLogged = true;
  traceExecution(TraceEvent.WELCOME_ANIMATION_PLAYING);
}

export function traceOobeLottieExecution() {
  if (firstOobeLottieEventLogged) {
    maybeTraceLastOobeLottieInitialization();
  } else {
    traceExecution(TraceEvent.FIRST_OOBE_LOTTIE_INITIALIZED);
    firstOobeLottieEventLogged = true;
  }
}

/**
 * Tracks the last <oobe-cr-lottie> initialization. Whenever this function is
 * called, it will create a timestamp and wait 'LAST_OOBE_LOTTIE_TIMEOUT_MSECS'
 * until writing it into the 'eventLog'. Any subsequent calls will clear the
 * timer so that only the last initialization will be actually stored.
 */
let scheduledLastOobeLottieTrace = null;
function maybeTraceLastOobeLottieInitialization() {
  // Amount of time to wait until logging the last OOBE Lottie Initialization.
  const LAST_OOBE_LOTTIE_TIMEOUT_MSECS = 5 * 1000;

  if (scheduledLastOobeLottieTrace) {
    clearTimeout(scheduledLastOobeLottieTrace);
  }

  const entry = createEventEntry(TraceEvent.LAST_OOBE_LOTTIE_INITIALIZED);
  scheduledLastOobeLottieTrace =
    setTimeout(() => eventLogs.push(entry), LAST_OOBE_LOTTIE_TIMEOUT_MSECS);
}
