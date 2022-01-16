// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assertInstanceof,
} from '../../assert.js';
import * as error from '../../error.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import {ScreenState} from '../../mojo/type.js';
import * as state from '../../state.js';
import {
  ErrorLevel,
  ErrorType,
  Mode,
} from '../../type.js';
import {windowController} from '../../window_controller.js';

/**
 * Manges usage of all camera operations.
 * TODO(b/209726472): Move more camera logic in camera view to here.
 */
export class CameraManager {
  private hasExternalScreen = false;
  private screenOffAuto = false;
  /**
   * The last time of all screen state turning from OFF to ON during the app
   * execution. Sets to -Infinity for no such time since app is opened.
   */
  private lastScreenOnTime = -Infinity;

  /**
   * Whether the device is in locked state.
   */
  private locked = false;

  private suspendRequested = false;

  constructor(private readonly doReconfiguration: () => Promise<boolean>) {
    // Monitor the states to stop camera when locked/minimized.
    const idleDetector = new IdleDetector();
    idleDetector.addEventListener('change', () => {
      this.locked = idleDetector.screenState === 'locked';
      if (this.locked) {
        this.doReconfiguration();
      }
    });
    idleDetector.start().catch((e) => {
      error.reportError(
          ErrorType.IDLE_DETECTOR_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    });

    document.addEventListener('visibilitychange', () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (this.isTabletBackground() && !recording) {
        this.doReconfiguration();
      }
    });
  }

  /**
   * @return Whether window is put to background in tablet mode.
   */
  private isTabletBackground(): boolean {
    return state.get(state.State.TABLET) &&
        document.visibilityState === 'hidden';
  }

  /**
   * @return If the App window is invisible to user with respect to screen off
   *     state.
   */
  private get screenOff(): boolean {
    return this.screenOffAuto && !this.hasExternalScreen;
  }

  async initialize(): Promise<void> {
    const helper = ChromeHelper.getInstance();

    const setTablet = (isTablet) => state.set(state.State.TABLET, isTablet);
    const isTablet = await helper.initTabletModeMonitor(setTablet);
    setTablet(isTablet);

    const handleScreenStateChange = () => {
      if (this.screenOff) {
        this.doReconfiguration();
      } else {
        this.lastScreenOnTime = performance.now();
      }
    };

    const updateScreenOffAuto = (screenState) => {
      const isOffAuto = screenState === ScreenState.OFF_AUTO;
      if (this.screenOffAuto !== isOffAuto) {
        this.screenOffAuto = isOffAuto;
        handleScreenStateChange();
      }
    };
    const screenState =
        await helper.initScreenStateMonitor(updateScreenOffAuto);
    updateScreenOffAuto(screenState);

    const updateExternalScreen = (hasExternalScreen) => {
      if (this.hasExternalScreen !== hasExternalScreen) {
        this.hasExternalScreen = hasExternalScreen;
        handleScreenStateChange();
      }
    };
    const hasExternalScreen =
        await helper.initExternalScreenMonitor(updateExternalScreen);
    updateExternalScreen(hasExternalScreen);
  }

  getLastScreenOnTime(): number {
    return this.lastScreenOnTime;
  }

  requestSuspend(): Promise<boolean> {
    state.set(state.State.SUSPEND, true);
    this.suspendRequested = true;
    return this.doReconfiguration();
  }

  requestResume(): void {
    state.set(state.State.SUSPEND, false);
    this.suspendRequested = false;
  }

  /**
   * Whether app window is suspended.
   */
  shouldSuspended(): boolean {
    return this.locked || windowController.isMinimized() ||
        this.suspendRequested || this.screenOff || this.isTabletBackground();
  }
}
