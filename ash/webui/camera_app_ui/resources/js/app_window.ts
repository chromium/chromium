// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ErrorInfo,
  PerfEntry,
  PerfEvent,
  Resolution,
} from './type.js';
import {WaitableEvent} from './waitable_event.js';

const TOP_BAR_HEIGHT = 32;

const DEFAULT_WINDOW_WIDTH = 764;

/**
 * Gets default window size which minimizes the letterbox area for given preview
 * aspect ratio.
 *
 * @param aspectRatio Preview aspect ratio.
 */
export function getDefaultWindowSize(aspectRatio: number): Resolution {
  // For the call site from background.js cannot access letterbox space reserved
  // for controls on 3 sides around #preview-box directly from dom. The
  // letterbox size number is hard coded here.
  // TODO(b/172345161): Reference these number from left, right, bottom of
  // #preview-box's bounding client rect after background.js being removed.
  const bottom = 88;
  const left = 88;
  const right = 100;

  return new Resolution(
      DEFAULT_WINDOW_WIDTH,
      Math.round(
          (DEFAULT_WINDOW_WIDTH - (left + right)) / aspectRatio + bottom +
          TOP_BAR_HEIGHT));
}

/**
 * Class which is used to coordinate the setup of window between Tast side and
 * CCA side.
 */
export class AppWindow {
  /**
   * A waitable event which will resolve to the URL of the CCA instance just
   * launched.
   */
  private readonly readyOnCCASide = new WaitableEvent<string>();

  private readonly readyOnTastSide = new WaitableEvent();

  private readonly onClosed = new WaitableEvent();

  private inClosingItself = false;

  private readonly errors: ErrorInfo[] = [];

  private readonly perfs: PerfEntry[] = [];

  private readonly launchedTime = performance.now();

  /**
   * @param fromColdStart Whether this app is launched from a cold start. It is
   *     used for performance measurement.
   */
  constructor(private readonly fromColdStart: boolean) {}

  /**
   * Waits until the window is bound and returns the URL of the window.
   *
   * @return The URL of the window.
   */
  waitUntilWindowBound(): Promise<string> {
    return this.readyOnCCASide.wait();
  }

  /**
   * Binds the URL to the window.
   */
  bindUrl(url: string): void {
    this.readyOnCCASide.signal(url);
  }

  /**
   * Notifies the listener that the window setup is done on Tast side.
   */
  notifyReadyOnTastSide(): void {
    this.readyOnTastSide.signal();
  }

  /**
   * Waits until the setup for the window is done on Tast side.
   */
  waitUntilReadyOnTastSide(): Promise<void> {
    return this.readyOnTastSide.wait();
  }

  /**
   * Triggers when CCA is fully launched.
   */
  onAppLaunched(): void {
    const event = this.fromColdStart ?
        PerfEvent.LAUNCHING_FROM_LAUNCH_APP_COLD :
        PerfEvent.LAUNCHING_FROM_LAUNCH_APP_WARM;
    this.perfs.push({
      event: event,
      duration: (performance.now() - this.launchedTime),
      perfInfo: {},
    });
  }

  /**
   * Notifies the listener that the window is closed.
   */
  notifyClosed(): void {
    this.onClosed.signal();
  }

  /**
   * Waits until the window is closed.
   */
  waitUntilClosed(): Promise<void> {
    return this.onClosed.wait();
  }

  /**
   * Notifies the listener that the window is about to close itself.
   */
  notifyClosingItself(): void {
    this.inClosingItself = true;
  }

  /**
   * Check if it has received the signal that the window is about to close
   * itself.
   */
  isClosingItself(): boolean {
    return this.inClosingItself;
  }

  /**
   * Reports error and makes it visible on Tast side.
   */
  reportError(errorInfo: ErrorInfo): void {
    this.errors.push(errorInfo);
  }

  /**
   * Gets all the errors.
   */
  getErrors(): ErrorInfo[] {
    return this.errors;
  }

  /**
   * Reports perf information and makes it visible on Tast side.
   *
   * @param perfEntry Information of the perf event.
   */
  reportPerf(perfEntry: PerfEntry): void {
    this.perfs.push(perfEntry);
  }

  /**
   * Gets all the perf information.
   */
  getPerfs(): PerfEntry[] {
    return this.perfs;
  }
}
