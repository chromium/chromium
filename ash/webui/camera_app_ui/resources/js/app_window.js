// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ErrorInfo,  // eslint-disable-line no-unused-vars
  PerfEntry,  // eslint-disable-line no-unused-vars
  PerfEvent,
  Resolution,
} from './type.js';
import {WaitableEvent} from './waitable_event.js';

const TOP_BAR_HEIGHT = 32;

const DEFAULT_WINDOW_WIDTH = 764;

/**
 * Gets default window size which minimizes the letterbox area for given preview
 * aspect ratio.
 * @param {number} aspectRatio Preview aspect ratio.
 * @return {!Resolution}
 */
export function getDefaultWindowSize(aspectRatio) {
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
 * CCA side. Note that the methods in this class are all marked with "async"
 * since the instance of this class will be wrapped by Comlink, which makes
 * all synchronous calls asynchronous. Making them async will make type check
 * easier.
 */
export class AppWindow {
  /**
   * @param {boolean} fromColdStart Whether this app is launched from a cold
   *     start. It is used for performance measurement.
   * @public
   */
  constructor(fromColdStart) {
    /**
     * @type {boolean}
     * @private
     */
    this.fromColdStart_ = fromColdStart;

    /**
     * A waitable event which will resolve to the URL of the CCA instance just
     * launched.
     * @type {!WaitableEvent<string>}
     * @private
     */
    this.readyOnCCASide_ = new WaitableEvent();

    /**
     * @type {!WaitableEvent}
     * @private
     */
    this.readyOnTastSide_ = new WaitableEvent();

    /**
     * @type {!WaitableEvent}
     * @private
     */
    this.onClosed_ = new WaitableEvent();

    /**
     * @type {boolean}
     * @private
     */
    this.inClosingItself_ = false;

    /**
     * @type {!Array<!ErrorInfo>}
     */
    this.errors_ = [];

    /**
     * @type {!Array<!PerfEntry>}
     */
    this.perfs_ = [];

    /**
     * @type {number}
     */
    this.launchedTime_ = performance.now();
  }

  /**
   * Waits until the window is bound and returns the URL of the window.
   * @return {!Promise<string>} The URL of the window.
   */
  async waitUntilWindowBound() {
    return this.readyOnCCASide_.wait();
  }

  /**
   * Binds the URL to the window.
   * @param {string} url
   * @return {!Promise}
   */
  async bindUrl(url) {
    this.readyOnCCASide_.signal(url);
  }

  /**
   * Notifies the listener that the window setup is done on Tast side.
   * @return {!Promise}
   */
  async notifyReadyOnTastSide() {
    this.readyOnTastSide_.signal();
  }

  /**
   * Waits until the setup for the window is done on Tast side.
   * @return {!Promise}
   */
  async waitUntilReadyOnTastSide() {
    return this.readyOnTastSide_.wait();
  }

  /**
   * Triggers when CCA is fully launched.
   * @return {!Promise}
   */
  async onAppLaunched() {
    const event = this.fromColdStart_ ?
        PerfEvent.LAUNCHING_FROM_LAUNCH_APP_COLD :
        PerfEvent.LAUNCHING_FROM_LAUNCH_APP_WARM;
    this.perfs_.push({
      event: event,
      duration: (performance.now() - this.launchedTime_),
    });
  }

  /**
   * Notifies the listener that the window is closed.
   * @return {!Promise}
   */
  async notifyClosed() {
    this.onClosed_.signal();
  }

  /**
   * Waits until the window is closed.
   * @return {!Promise}
   */
  async waitUntilClosed() {
    return this.onClosed_.wait();
  }

  /**
   * Notifies the listener that the window is about to close itself.
   * @return {!Promise}
   */
  async notifyClosingItself() {
    this.inClosingItself_ = true;
  }

  /**
   * Check if it has received the signal that the window is about to close
   * itself.
   * @return {!Promise<boolean>}
   */
  async isClosingItself() {
    return this.inClosingItself_;
  }

  /**
   * Reports error and makes it visible on Tast side.
   * @param {!ErrorInfo} errorInfo Information of the error.
   * @return {!Promise}
   */
  async reportError(errorInfo) {
    this.errors_.push(errorInfo);
  }

  /**
   * Gets all the errors.
   * @return {!Promise<!Array<!ErrorInfo>>}
   */
  async getErrors() {
    return this.errors_;
  }

  /**
   * Reports perf information and makes it visible on Tast side.
   * @param {!PerfEntry} perfEntry Information of the perf event.
   * @return {!Promise}
   */
  async reportPerf(perfEntry) {
    this.perfs_.push(perfEntry);
  }

  /**
   * Gets all the perf information.
   * @return {!Promise<!Array<!PerfEntry>>}
   */
  async getPerfs() {
    return this.perfs_;
  }
}
