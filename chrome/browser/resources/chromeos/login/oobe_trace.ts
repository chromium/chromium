// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OOBE Tracing Utilities
 *
 * This file contain utilities for measuring OOBE's frontend load timings using
 * the Performance API. The timestamps are created using performance.now() which
 * provides us with the number of milliseconds since `performance.timeOrigin`.
 *
 * For a typical web page, running `performance.now()` as the first script in
 * the page gives values that are close enough to zero (typically less than a
 * few dozen miliseconds) that `timeOrigin` is a reliable reference for timing
 * the execution of scripts. In a WebUI like OOBE, this is not the case. That is
 * because `performance.timeOrigin` is set when the BrowserContext is created,
 * which happens much earlier than the first JavaScript instruction. For OOBE,
 * running `performance.now()` as the first thing in the page reports ~ 1000ms.
 *
 * In order to account for this discrepancy, OOBE executes oobe_trace_start.js
 * as its first script to set the `FIRST_INSTRUCTION` TraceEvent.
 *
 * For now, the results are written into 'window' as 'oobeTraceLogs'. In the
 * future, these values will be available directly from the OOBE debugger and
 * will be written into an UMA metric.
 */

import {loadTimeData} from './i18n_setup.js';

export enum TraceEvent {
  FIRST_INSTRUCTION = 'FIRST_INSTRUCTION',
  FIRST_LINE_AFTER_IMPORTS = 'FIRST_LINE_AFTER_IMPORTS',
  PRIORITY_SCREENS_ADDED = 'PRIORITY_SCREENS_ADDED',
  COMMON_SCREENS_ADDED = 'COMMON_SCREENS_ADDED',
  REMAINING_SCREENS_ADDED = 'REMAINING_SCREENS_ADDED',
  DOM_CONTENT_LOADED = 'DOM_CONTENT_LOADED',
  OOBE_INITIALIZED = 'OOBE_INITIALIZED',
  FIRST_SCREEN_SHOWN = 'FIRST_SCREEN_SHOWN',
  FIRST_OOBE_LOTTIE_INITIALIZED = 'FIRST_OOBE_LOTTIE_INITIALIZED',
  LAST_OOBE_LOTTIE_INITIALIZED = 'LAST_OOBE_LOTTIE_INITIALIZED',
  WELCOME_ANIMATION_PLAYING = 'WELCOME_ANIMATION_PLAYING',
}

// Event log containing a TraceEvent and a timestamp.
const eventLogs: EventEntry[] = [];
window.oobeTraceLogs = eventLogs;

class EventEntry {
  name: string;
  delta: number;

  constructor(traceEventName: TraceEvent) {
    this.name = traceEventName;
    this.delta = performance.now();
  }
}

if (window.oobeInitializationBeginTimestamp) {
  const oobeTimeOrigin = new EventEntry(TraceEvent.FIRST_INSTRUCTION);
  oobeTimeOrigin.delta = window.oobeInitializationBeginTimestamp;
  eventLogs.push(oobeTimeOrigin);
}

let firstScreenShownEventLogged = false;
let welcomeAnimationPlayEventLogged = false;
let firstOobeLottieEventLogged = false;

export function traceExecution(traceEvent: TraceEvent): void {
  eventLogs.push(new EventEntry(traceEvent));
}

export function traceFirstScreenShown(): void {
  if (firstScreenShownEventLogged) {
    return;
  }
  firstScreenShownEventLogged = true;
  traceExecution(TraceEvent.FIRST_SCREEN_SHOWN);
}

export function traceWelcomeAnimationPlay(): void {
  if (welcomeAnimationPlayEventLogged) {
    return;
  }
  welcomeAnimationPlayEventLogged = true;
  traceExecution(TraceEvent.WELCOME_ANIMATION_PLAYING);
}

export function traceOobeLottieExecution(): void {
  if (!firstOobeLottieEventLogged) {
    traceExecution(TraceEvent.FIRST_OOBE_LOTTIE_INITIALIZED);
    firstOobeLottieEventLogged = true;
  }
  maybeTraceLastOobeLottieInitialization();
}

/**
 * Tracks the last <oobe-cr-lottie> initialization. Whenever this function is
 * called, it will create a timestamp and wait 'LAST_OOBE_LOTTIE_TIMEOUT_MSECS'
 * until writing it into the 'eventLog'. Any subsequent calls will clear the
 * timer so that only the last initialization will be actually stored.
 */
let scheduledLastOobeLottieTrace: number = 0;
function maybeTraceLastOobeLottieInitialization(): void {
  // Amount of time to wait until logging the last OOBE Lottie Initialization.
  const LAST_OOBE_LOTTIE_TIMEOUT_MSECS = 3 * 1000;
  clearTimeout(scheduledLastOobeLottieTrace);

  const entry = new EventEntry(TraceEvent.LAST_OOBE_LOTTIE_INITIALIZED);
  scheduledLastOobeLottieTrace = setTimeout(() => {
    eventLogs.push(entry);
    maybePrintTraces();
  }, LAST_OOBE_LOTTIE_TIMEOUT_MSECS);
}

// Maybe output the timings to the console to be parsed when the command line
// switch 'oobe-print-frontend-timings' is present. More details can be found in
// go/oobe-frontend-trace-timings
function maybePrintTraces(): void {
  if (!loadTimeData.valueExists('printFrontendTimings') ||
      !loadTimeData.getBoolean('printFrontendTimings')) {
    return;
  }

  const EventPrintOrder = [
    TraceEvent.FIRST_INSTRUCTION,
    TraceEvent.FIRST_LINE_AFTER_IMPORTS,
    TraceEvent.PRIORITY_SCREENS_ADDED,
    TraceEvent.COMMON_SCREENS_ADDED,
    TraceEvent.REMAINING_SCREENS_ADDED,
    TraceEvent.DOM_CONTENT_LOADED,
    TraceEvent.OOBE_INITIALIZED,
    TraceEvent.FIRST_SCREEN_SHOWN,
    TraceEvent.FIRST_OOBE_LOTTIE_INITIALIZED,
    TraceEvent.LAST_OOBE_LOTTIE_INITIALIZED,
    TraceEvent.WELCOME_ANIMATION_PLAYING,
  ];

  let output = 'OOBE_TRACE_BEGIN_';
  for (const eventName of EventPrintOrder) {
    const matchingEvent = eventLogs.find(e => e.name === eventName);
    output += matchingEvent ? matchingEvent.delta + ';' : 'NaN;';
  }
  output += '_OOBE_TRACE_END';
  console.error(output);
}

declare global {
  interface Window {
    oobeInitializationBeginTimestamp: DOMHighResTimeStamp;
    oobeTraceLogs: EventEntry[];
  }
}
