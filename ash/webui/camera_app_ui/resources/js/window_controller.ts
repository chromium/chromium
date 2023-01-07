// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {
  WindowStateControllerRemote,
  WindowStateMonitorCallbackRouter,
  WindowStateType,
} from './mojo/type.js';
import {wrapEndpoint} from './mojo/util.js';

type WindowStateChangedEventListener = (states: WindowStateType[]) => void;

/**
 * Controller to get/set/listener for window state.
 */
export class WindowController {
  /**
   * The remote controller from Mojo interface.
   */
  private windowStateController: WindowStateControllerRemote|null = null;

  /**
   * Current window states.
   */
  private windowStates: WindowStateType[] = [];

  /**
   * Set of the listeners for window state changed events.
   */
  private readonly listeners = new Set<WindowStateChangedEventListener>();

  /**
   * Binds the controller remote from Mojo interface.
   */
  async bind(remoteController: WindowStateControllerRemote): Promise<void> {
    this.windowStateController = remoteController;

    const windowMonitorCallbackRouter =
        wrapEndpoint(new WindowStateMonitorCallbackRouter());
    windowMonitorCallbackRouter.onWindowStateChanged.addListener(
        (states: WindowStateType[]) => {
          this.windowStates = states;
          for (const listener of this.listeners) {
            listener(states);
          }
        });
    const {states} = await this.windowStateController.addMonitor(
        windowMonitorCallbackRouter.$.bindNewPipeAndPassRemote());
    this.windowStates = states;
  }

  /**
   * Minimizes the window.
   */
  minimize(): Promise<void> {
    return assertInstanceof(
               this.windowStateController, WindowStateControllerRemote)
        .minimize();
  }

  /**
   * Maximizes the window.
   */
  maximize(): Promise<void> {
    return assertInstanceof(
               this.windowStateController, WindowStateControllerRemote)
        .maximize();
  }

  /**
   * Restores the window and leaves maximized/minimized/fullscreen state.
   */
  restore(): Promise<void> {
    return assertInstanceof(
               this.windowStateController, WindowStateControllerRemote)
        .restore();
  }

  /**
   * Makes the window fullscreen.
   */
  fullscreen(): Promise<void> {
    return assertInstanceof(
               this.windowStateController, WindowStateControllerRemote)
        .fullscreen();
  }

  /**
   * Focuses the window.
   */
  focus(): Promise<void> {
    return assertInstanceof(
               this.windowStateController, WindowStateControllerRemote)
        .focus();
  }

  /**
   * Returns true if the window is currently minimized.
   */
  isMinimized(): boolean {
    return this.windowStates.includes(WindowStateType.MINIMIZED);
  }

  /**
   * Returns true if the window is currently fullscreen or maximized.
   */
  isFullscreenOrMaximized(): boolean {
    return this.windowStates.includes(WindowStateType.FULLSCREEN) ||
        this.windowStates.includes(WindowStateType.MAXIMIZED);
  }

  /**
   * Adds listener for the window state changed events.
   */
  addListener(listener: WindowStateChangedEventListener): void {
    this.listeners.add(listener);
  }
}

export const windowController = new WindowController();
