// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';
import {ErrorType, Mode} from './switch_access_constants.js';

/**
 * Class to handle auto-scan behavior.
 */
export class AutoScanManager {
  private intervalID_?: number;
  private isEnabled_ = false;
  /** Whether the current node is within the virtual keyboard. */
  private inKeyboard_ = false;
  /** Auto-scan interval for the on-screen keyboard in milliseconds. */
  private keyboardScanTime_ = NOT_INITIALIZED;
  /** Length of the auto-scan interval for most contexts, in milliseconds. */
  private primaryScanTime_ = NOT_INITIALIZED;

  static instance?: AutoScanManager;

  private constructor() {}

  // ============== Static Methods ================

  static init(): void {
    if (AutoScanManager.instance) {
      throw SwitchAccess.error(
          ErrorType.DUPLICATE_INITIALIZATION,
          'Cannot call AutoScanManager.init() more than once.');
    }
    AutoScanManager.instance = new AutoScanManager();
  }

  /** Restart auto-scan under current settings if it is currently running. */
  static restartIfRunning(): void {
    if (AutoScanManager.instance?.isRunning_()) {
      AutoScanManager.instance.stop_();
      AutoScanManager.instance.start_();
    }
  }

  /**
   * Stop auto-scan if it is currently running. Then, if |enabled| is true,
   * turn on auto-scan. Otherwise leave it off.
   */
  static setEnabled(enabled: boolean): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (AutoScanManager.instance!.isRunning_()) {
      AutoScanManager.instance!.stop_();
    }
    AutoScanManager.instance!.isEnabled_ = enabled;
    if (enabled) {
      AutoScanManager.instance!.start_();
    }
  }

  /** Sets whether the keyboard scan time is used. */
  static setInKeyboard(inKeyboard: boolean): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    AutoScanManager.instance!.inKeyboard_ = inKeyboard;
  }

  /** Update this.keyboardScanTime_ to |scanTime|, in milliseconds. */
  static setKeyboardScanTime(scanTime: number): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    AutoScanManager.instance!.keyboardScanTime_ = scanTime;
    if (AutoScanManager.instance!.inKeyboard_) {
      AutoScanManager.restartIfRunning();
    }
  }

  /**
   * Update this.primaryScanTime_ to |scanTime|. Then, if auto-scan is currently
   * running, restart it.
   * @param scanTime Auto-scan interval time in milliseconds.
   */
  static setPrimaryScanTime(scanTime: number): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    AutoScanManager.instance!.primaryScanTime_ = scanTime;
    AutoScanManager.restartIfRunning();
  }

  // ============== Private Methods ================

  /** Return true if auto-scan is currently running. Otherwise return false. */
  private isRunning_(): boolean {
    return this.isEnabled_;
  }

  /**
   * Set the window to move to the next node at an interval in milliseconds
   * depending on where the user is navigating. Currently,
   * this.keyboardScanTime_ is used as the interval if the user is
   * navigating in the virtual keyboard, and this.primaryScanTime_ is used
   * otherwise. Does not do anything if AutoScanManager is already scanning.
   */
  private start_(): void {
    if (this.primaryScanTime_ === NOT_INITIALIZED || this.intervalID_ ||
        SwitchAccess.mode === Mode.POINT_SCAN) {
      return;
    }

    let currentScanTime = this.primaryScanTime_;

    if (SwitchAccess.improvedTextInputEnabled() && this.inKeyboard_ &&
        this.keyboardScanTime_ !== NOT_INITIALIZED) {
      currentScanTime = this.keyboardScanTime_;
    }

    this.intervalID_ = setInterval(() => {
      if (SwitchAccess.mode === Mode.POINT_SCAN) {
        this.stop_();
        return;
      }
      Navigator.byItem.moveForward();
    }, currentScanTime);
  }

  /** Stop the window from moving to the next node at a fixed interval. */
  private stop_(): void {
    clearInterval(this.intervalID_);
    this.intervalID_ = undefined;
  }
}

// Private to module.

/** Sentinel value that indicates an uninitialized scan time. */
const NOT_INITIALIZED = -1;

TestImportManager.exportForTesting(AutoScanManager);
