// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import type {MetricsReporter} from '//resources/js/metrics_reporter/metrics_reporter.js';

import type {BrowserProxy} from './browser_proxy.js';


/**
 * Input type to activate the ReloadButton.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * Defined in //tools/metrics/histograms/metadata/ui/enums.xml.
 */
// TODO(crbug.com/448794588): Force syncing with C++ side once finalized.
export enum ReloadButtonInputType {
  MOUSE_RELEASE = 0,
  KEY_PRESS = 1,
  MAX_VALUE = 2,
}

/**
 * The visible mode of the ReloadButton.
 */
export enum ReloadButtonVisibleMode {
  // The "refresh" icon.
  RELOAD,
  // The "clear" icon.
  STOP,
}

// Histogram names.
const INPUT_COUNT_HISTOGRAM_ = 'InitialWebUI.ReloadButton.InputCount';
const MOUSE_HOVER_TO_NEXT_PAINT_HISTOGRAM_ =
    'InitialWebUI.ReloadButton.MouseHoverToNextPaint';
const MOUSE_PRESS_TO_NEXT_PAINT_HISTOGRAM_ =
    'InitialWebUI.ReloadButton.MousePressToNextPaint';
const INPUT_TO_NEXT_PAINT_MOUSE_RELEASE_HISTOGRAM_ =
    'InitialWebUI.ReloadButton.InputToNextPaint.MouseRelease';

/**
 * Class responsible for recording metrics related to the ReloadButton WebUI.
 */
export class MetricsRecorder {
  /**
   * Returns the visible mode based on the given loading state.
   * @param isLoading Whether the page is currently loading.
   * @return The target visible mode.
   */
  static getVisibleMode(isLoading: boolean): ReloadButtonVisibleMode {
    return isLoading ? ReloadButtonVisibleMode.STOP :
                       ReloadButtonVisibleMode.RELOAD;
  }

  private browserProxy_: BrowserProxy;
  private metricsReporter_: MetricsReporter;

  /**
   * The obserer to monitor event timings for interaction-to-next-paint metrics.
   * Null if not currently observing.
   */
  private interactionObserver_: PerformanceObserver|null = null;

  /**
   * Stores the type of the last input event that triggered the button press.
   * Null if no input event has been processed yet or the last event was not
   * relevant.
   */
  private lastInputType_: ReloadButtonInputType|null = null;

  /**
   * @param browserProxy The browser proxy instance.
   */
  constructor(browserProxy: BrowserProxy) {
    this.browserProxy_ = browserProxy;
    this.metricsReporter_ = MetricsReporterImpl.getInstance();
  }

  /**
   * Starts observing interaction events for metrics recording if not already
   * doing so.
   */
  startObserving() {
    if (!this.interactionObserver_) {
      this.interactionObserver_ = this.observeInteractionsToNextPaint_(
          ['mouseenter', 'mousedown', 'click', 'keydown']);
    }
  }

  /**
   * Stops observing interaction events for metrics recording.
   */
  stopObserving() {
    if (this.interactionObserver_) {
      this.interactionObserver_.disconnect();
      this.interactionObserver_ = null;
    }
  }

  /**
   * Called when the button is pressed, either by mouse click or key press.
   * Records the input type if it's a relevant event (click or Enter key).
   * @param event The event that triggered the button press.
   */
  onButtonPressedStart(event: Event) {
    let inputType: ReloadButtonInputType|null = null;
    // TODO(crbug.com/448794588): The C++ side is currently recorded with
    // mouserelease. Need to unify them.
    if (event instanceof PointerEvent) {
      inputType = ReloadButtonInputType.MOUSE_RELEASE;
    } else if (event instanceof KeyboardEvent) {
      // TODO(crbug.com/448794588): Need an event listener for keypress.
      inputType = ReloadButtonInputType.KEY_PRESS;
    } else {
      return;
    }

    this.lastInputType_ = inputType;
    this.browserProxy_.recordInHistogram(
        INPUT_COUNT_HISTOGRAM_, inputType, ReloadButtonInputType.MAX_VALUE);
  }

  /**
   * Called when the button's visible mode is changed.
   * @param currentMode The current visible mode.
   * @param targetMode The target visible mode to change to.
   */
  onChangeVisibleMode(
      currentMode: ReloadButtonVisibleMode,
      targetMode: ReloadButtonVisibleMode) {
    if (currentMode === targetMode) {
      return;
    }

    // TODO(crbug.com/448794588): Implement a proper way to wait for the next
    // paint on changing the visible icon.
  }

  /**
   * Calculates the time of the next paint for a given performance entry.
   *
   * Browsers may round event durations down to the nearest 8ms. To ensure the
   * paint time doesn't illogically appear to occur before processing starts,
   * we clamp it using Math.max().
   * @param entry The PerformanceEventTiming entry.
   * @return The time of the next paint.
   */
  private getNextPaintTime_(entry: PerformanceEventTiming):
      DOMHighResTimeStamp {
    return Math.max(entry.startTime + entry.duration, entry.processingStart);
  }

  /**
   * Converts a time in milliseconds to microseconds.
   * @param timeInMs The time in milliseconds.
   * @return The duration in microseconds.
   */
  private toMicroseconds_(timeInMs: number): bigint {
    return BigInt(Math.round(timeInMs * 1000));
  }

  /**
   * Callback function for the PerformanceObserver. Processes
   * PerformanceEventTiming entries to calculate and record
   * interaction-to-next-paint metrics.
   * @param entry The PerformanceEventTiming entry to process.
   */
  private onPerformanceEventEntry_(entry: PerformanceEventTiming) {
    const duration = this.getNextPaintTime_(entry) - entry.startTime;
    const durationInMicroseconds = this.toMicroseconds_(duration);

    switch (entry.name) {
      case 'mouseenter':
        this.metricsReporter_.umaReportTime(
            MOUSE_HOVER_TO_NEXT_PAINT_HISTOGRAM_, durationInMicroseconds);
        break;
      case 'mousedown':
        this.metricsReporter_.umaReportTime(
            MOUSE_PRESS_TO_NEXT_PAINT_HISTOGRAM_, durationInMicroseconds);
        break;
      case 'click':
        // TODO(crbug.com/448794588): This should really be associated with
        // mouse release event after `onButtonPressedStart()` is fixed.
        if (this.lastInputType_ === ReloadButtonInputType.MOUSE_RELEASE) {
          this.metricsReporter_.umaReportTime(
              INPUT_TO_NEXT_PAINT_MOUSE_RELEASE_HISTOGRAM_,
              durationInMicroseconds);
          this.lastInputType_ = null;  // Clear after use
        }
        break;
      case 'keydown':
        // TODO(crbug.com/448794588): Only record if this keydown is an
        // activating key (Enter/Space) for the button. This requires a call
        // to `onButtonPressedStart()` from app.ts's keydown handler.
        break;
      default:
    }
  }

  /**
   * Callback for the interaction observer. It filters and processes entries
   * from the observer's entry list.
   * @param list The list of performance entries.
   * @param eventNames The event names to filter by.
   */
  private onObserveInteractionEntryList_(
      list: PerformanceObserverEntryList, eventNames: string[]) {
    for (const entry of list.getEntries()) {
      if (eventNames.includes(entry.name)) {
        this.onPerformanceEventEntry_(entry as PerformanceEventTiming);
      }
    }
  }

  /**
   * Initializes and starts a PerformanceObserver to listen for specified event
   * types and measure the time to the next paint for each.
   * @param eventNames An array of event names to observe.
   * @return The created PerformanceObserver instance or null if not supported.
   */
  private observeInteractionsToNextPaint_(eventNames: string[]):
      PerformanceObserver|null {
    const observer = new PerformanceObserver(
        (list) => this.onObserveInteractionEntryList_(list, eventNames));

    observer.observe({
      // The Event Timing API uses the entry type 'event' for user interactions.
      type: 'event',
      // Ensures we capture all previous events.
      buffered: true,
      // Ensures we capture shortest possible duration.
      durationThreshold: 0,
    });
    return observer;
  }
}
