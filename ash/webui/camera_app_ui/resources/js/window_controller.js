// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {
  WindowStateControllerRemote,
  WindowStateMonitorCallbackRouter,
  WindowStateType,
} from './mojo/type.js';
import {wrapEndpoint} from './mojo/util.js';

/**
 * @typedef {function(!Array<!WindowStateType>): void}
 */
let WindowStateChangedEventListener;  // eslint-disable-line no-unused-vars

/**
 * Controller to get/set/listener for window state.
 */
export class WindowController {
  /**
   * @public
   */
  constructor() {
    /**
     * The remote controller from Mojo interface.
     * @type {?WindowStateControllerRemote}
     */
    this.windowStateController_ = null;

    /**
     * Current window states.
     * @type {!Array<!WindowStateType>}
     */
    this.windowStates_ = [];

    /**
     * Set of the listeners for window state changed events.
     * @type {!Set<!WindowStateChangedEventListener>}
     */
    this.listeners_ = new Set();
  }

  /**
   * Binds the controller remote from Mojo interface.
   * @param {!WindowStateControllerRemote} remoteController
   * @return {!Promise}
   */
  async bind(remoteController) {
    this.windowStateController_ = remoteController;

    const windowMonitorCallbackRouter =
        wrapEndpoint(new WindowStateMonitorCallbackRouter());
    windowMonitorCallbackRouter.onWindowStateChanged.addListener((states) => {
      this.windowStates_ = states;
      this.listeners_.forEach((listener) => listener(states));
    });
    const {states} = await this.windowStateController_.addMonitor(
        windowMonitorCallbackRouter.$.bindNewPipeAndPassRemote());
    this.windowStates_ = states;
  }

  /**
   * Minimizes the window.
   * @return {!Promise}
   */
  async minimize() {
    return assertInstanceof(
               this.windowStateController_, WindowStateControllerRemote)
        .minimize();
  }

  /**
   * Maximizes the window.
   * @return {!Promise}
   */
  async maximize() {
    return assertInstanceof(
               this.windowStateController_, WindowStateControllerRemote)
        .maximize();
  }

  /**
   * Restores the window and leaves maximized/minimized/fullscreen state.
   * @return {!Promise}
   */
  async restore() {
    return assertInstanceof(
               this.windowStateController_, WindowStateControllerRemote)
        .restore();
  }

  /**
   * Makes the window fullscreen.
   * @return {!Promise}
   */
  async fullscreen() {
    return assertInstanceof(
               this.windowStateController_, WindowStateControllerRemote)
        .fullscreen();
  }

  /**
   * Focuses the window.
   * @return {!Promise}
   */
  async focus() {
    return assertInstanceof(
               this.windowStateController_, WindowStateControllerRemote)
        .focus();
  }

  /**
   * Returns true if the window is currently minimized.
   * @return {boolean}
   */
  isMinimized() {
    return this.windowStates_.includes(WindowStateType.MINIMIZED);
  }

  /**
   * Returns true if the window is currently fullscreen or maximized.
   * @return {boolean}
   */
  isFullscreenOrMaximized() {
    return this.windowStates_.includes(WindowStateType.FULLSCREEN) ||
        this.windowStates_.includes(WindowStateType.MAXIMIZED);
  }

  /**
   * Adds listener for the window state (including window size) changed events.
   * @param {!WindowStateChangedEventListener} listener
   */
  addListener(listener) {
    this.listeners_.add(listener);
  }
}

export const windowController = new WindowController();
