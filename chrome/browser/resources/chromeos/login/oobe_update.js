// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

'use strict';

(function() {

const USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR = 'update-accept-cellular';
const USER_ACTION_REJECT_UPDATE_OVER_CELLUAR = 'update-reject-cellular';
const USER_ACTION_CANCEL_UPDATE_SHORTCUT = 'cancel-update';

const UNREACHABLE_PERCENT = 1000;
// Thresholds which are used to determine when update status announcement should
// take place. Last element is not reachable to simplify implementation.
const PERCENT_THRESHOLDS = [
  0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 98, 99, 100, UNREACHABLE_PERCENT
];

/**
 * Enum for the UI states corresponding to sub steps inside update screen.
 * These values must be kept in sync with string constants in
 * update_screen_handler.cc.
 * @enum {string}
 */
const UIState = {
  CHECKING: 'checking',
  UPDATE: 'update',
  RESTART: 'restart',
  REBOOT: 'reboot',
  CELLULAR: 'cellular',
};


Polymer({
  is: 'oobe-update-element',

  behaviors: [
    OobeI18nBehavior,
    MultiStepBehavior,
    LoginScreenBehavior
  ],

  EXTERNAL_API: [
    'setCancelUpdateShortcutEnabled',
    'setRequiresPermissionForCellular',
    'showLowBatteryWarningMessage',
    'setUpdateState',
    'setUpdateStatus',
    'setAutoTransition',
  ],

  properties: {
    /**
     * True if update is fully completed and manual action is required.
     */
    manualRebootNeeded: {
      type: Boolean,
      value: false,
    },

    /**
     * If update cancellation is allowed.
     */
    cancelAllowed: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of the localized string for update cancellation message.
     */
    cancelHint: {
      type: String,
      value: 'cancelUpdateHint',
    },

    /**
     * Shows battery warning message during Downloading stage.
     */
    showLowBatteryWarning: {
      type: Boolean,
      value: false,
    },

    /**
     * Message like "3% complete".
     */
    updateStatusMessagePercent: {
      type: String,
    },

    /**
     * Message like "About 5 minutes left".
     */
    updateStatusMessageTimeLeft: {
      type: String,
    },

    /**
     * Progress bar percent that is used in BetterUpdate version of the screen.
     */
    betterUpdateProgressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Whether auto-transition is enabled or not.
     */
    autoTransition: {
      type: Boolean,
      value: true,
    },

    /**
     * Index of threshold that has been already achieved.
     */
    thresholdIndex: {
      type: Number,
      value: 0,
    }
  },

  defaultUIStep() {
    return UIState.CHECKING;
  },

  UI_STEPS: UIState,

  ready() {
    this.initializeLoginScreen('UpdateScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Cancels the screen.
   */
  cancel() {
    this.userActed(USER_ACTION_CANCEL_UPDATE_SHORTCUT);
  },

  onBackClicked_() {
    this.userActed(USER_ACTION_REJECT_UPDATE_OVER_CELLUAR);
  },

  onNextClicked_() {
    this.userActed(USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR);
  },

  /** @param {boolean} enabled */
  setCancelUpdateShortcutEnabled(enabled) {
    this.cancelAllowed = enabled;
  },

  /**
   * Shows or hides the warning that asks the user for permission to update
   * over celluar.
   * @param {boolean} requiresPermission Are the warning visible?
   */
  setRequiresPermissionForCellular(requiresPermission) {
    this.requiresPermissionForCellular = requiresPermission;
  },

  /**
   * Shows or hides battery warning message.
   * @param {boolean} visible Is message visible?
   */
  showLowBatteryWarningMessage(visible) {
    this.showLowBatteryWarning = visible;
  },

  /**
   * Sets which dialog should be shown.
   * @param {UIState} value Current update state.
   */
  setUpdateState(value) {
    this.setUIStep(value);
  },

  /**
   * Sets percent to be shown in progress bar.
   * @param {number} percent Current progress
   * @param {string} messagePercent Message describing current progress.
   * @param {string} messageTimeLeft Message describing time left.
   */
  setUpdateStatus(percent, messagePercent, messageTimeLeft) {
    // Sets aria-live polite on percent and timeleft container every time new
    // threshold has been achieved otherwise do not initiate spoken feedback
    // update by setting aria-live off.
    if (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
      while (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
        this.thresholdIndex = this.thresholdIndex + 1;
      }
      this.$['better-update-percent'].setAttribute('aria-live', 'polite');
      this.$['better-update-timeleft'].setAttribute('aria-live', 'polite');
    } else {
      this.$['better-update-timeleft'].setAttribute('aria-live', 'off');
      this.$['better-update-percent'].setAttribute('aria-live', 'off');
    }
    this.betterUpdateProgressValue = percent;
    this.updateStatusMessagePercent = messagePercent;
    this.updateStatusMessageTimeLeft = messageTimeLeft;
  },

  /**
   * Sets whether carousel should auto transit slides.
   */
  setAutoTransition(value) {
    this.autoTransition = value;
  },

  /**
   * Gets whether carousel should auto transit slides.
   * @private
   * @param {UIState} step Which UIState is shown now.
   * @param {boolean} autoTransition Is auto transition allowed.
   */
  getAutoTransition_(step, autoTransition) {
    return step == UIState.UPDATE && autoTransition;
  },

});
})();
