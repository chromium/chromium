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
 * These values must be kept in sync with UpdateView::UIState in C++ code.
 * @enum {number}
 */
var UpdateUIState = {
  CHECKING_FOR_UPDATE: 0,
  UPDATE_IN_PROGRESS: 1,
  RESTART_IN_PROGRESS: 2,
  MANUAL_REBOOT: 3,
};


Polymer({
  is: 'oobe-update',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setEstimatedTimeLeft',
    'showEstimatedTimeLeft',
    'setUpdateCompleted',
    'showUpdateCurtain',
    'setProgressMessage',
    'setUpdateProgress',
    'setRequiresPermissionForCellular',
    'setCancelUpdateShortcutEnabled',
    'showLowBatteryWarningMessage',
    'setUIState',
    'setUpdateStatus',
    'setAutoTransition',
  ],

  properties: {
    /**
     * Shows better update screen instead of the old one. True when
     * kBetterUpdateScreen feature flag is enabled.
     */
    betterUpdateScreenFeatureEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('betterUpdateScreenFeatureEnabled');
      },
      readOnly: true,
    },

    /**
     * Shows "Checking for update ..." section and hides "Updating..." section.
     */
    checkingForUpdate: {
      type: Boolean,
      value: true,
    },

    /**
     * Shows a warning to the user the update is about to proceed over a
     * cellular network, and asks the user to confirm.
     */
    requiresPermissionForCellular: {
      type: Boolean,
      value: false,
    },

    /**
     * Progress bar percent.
     */
    progressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Estimated time left in seconds.
     */
    estimatedTimeLeft: {
      type: Number,
      value: 0,
    },

    /**
     * Shows estimatedTimeLeft.
     */
    estimatedTimeLeftShown: {
      type: Boolean,
    },

    /**
     * Message "33 percent done".
     */
    progressMessage: {
      type: String,
    },

    /**
     * True if update is fully completed and, probably manual action is
     * required.
     */
    updateCompleted: {
      type: Boolean,
      value: false,
    },

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
     * Current UI state which corresponds to a sub step in update process.
     */
    uiState: {type: Number, value: 0},

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

  ready() {
    this.initializeLoginScreen('UpdateScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Cancels the screen.
   */
  cancel() {
    this.cancelHint = 'cancelledUpdateMessage';
    this.userActed(USER_ACTION_CANCEL_UPDATE_SHORTCUT);
  },

  onBeforeShow() {
    if (!this.betterUpdateScreenFeatureEnabled_) {
      cr.ui.login.invokePolymerMethod(
          this.$['checking-downloading-update'], 'onBeforeShow');
    }
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
   * Sets update's progress bar value.
   * @param {number} progress Percentage of the progress bar.
   */
  setUpdateProgress(progress) {
    this.progressValue = progress;
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
   * Shows or hides downloading ETA message.
   * @param {boolean} visible Are ETA message visible?
   */
  showEstimatedTimeLeft(visible) {
    this.estimatedTimeLeftShown = visible;
  },

  /**
   * Sets estimated time left until download will complete.
   * @param {number} seconds Time left in seconds.
   */
  setEstimatedTimeLeft(seconds) {
    this.estimatedTimeLeft = seconds;
  },

  /**
   * Sets message below progress bar. Hide the message by setting an empty
   * string.
   * @param {string} message Message that should be shown.
   */
  setProgressMessage(message) {
    let visible = !!message;
    this.progressMessage = message;
    this.estimatedTimeLeftShown = !visible;
  },

  /**
   * Marks update completed. Shows "update completed" message.
   * @param {boolean} is_completed True if update process is completed.
   */
  setUpdateCompleted(is_completed) {
    this.updateCompleted = is_completed;
  },

  /**
   * Shows or hides update curtain.
   * @param {boolean} visible Are curtains visible?
   */
  showUpdateCurtain(visible) {
    this.checkingForUpdate = visible;
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
   * @param {UpdateUIState} value Current UI state.
   */
  setUIState(value) {
    this.uiState = value;
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
   * @param {UpdateUIState} state Which UIState now.
   * @param {boolean} autoTransition Is auto transition allowed.
   */
  getAutoTransition_(state, autoTransition) {
    return state == UpdateUIState.UPDATE_IN_PROGRESS && autoTransition;
  },

  /**
   * Sets whether checking for update dialog is shown.
   * @private
   * @param {UpdateUIState} state Which UIState now.
   * @param {boolean} requiresPermission Is permission update dialog shown?
   */
  isCheckingForUpdate_(state, requiresPermission) {
    return state == UpdateUIState.CHECKING_FOR_UPDATE && !requiresPermission;
  },

  /**
   * Sets whether update in progress dialog is shown.
   * @private
   * @param {UpdateUIState} state Which UIState now.
   * @param {boolean} requiresPermission Is permission update dialog shown?
   */
  isUpdateInProgress_(state, requiresPermission) {
    return state == UpdateUIState.UPDATE_IN_PROGRESS && !requiresPermission;
  },

  /**
   * Sets whether restart in progress dialog is shown.
   * @private
   * @param {UpdateUIState} state Which UIState now.
   * @param {boolean} requiresPermission Is permission update dialog shown?
   */
  isRestartInProgress_(state, requiresPermission) {
    return state == UpdateUIState.RESTART_IN_PROGRESS && !requiresPermission;
  },

  /**
   * Sets whether manual reboot dialog is shown.
   * @private
   * @param {UpdateUIState} state Which UIState now.
   * @param {boolean} requiresPermission Is permission update dialog shown?
   */
  isManualReboot_(state, requiresPermission) {
    return state == UpdateUIState.MANUAL_REBOOT && !requiresPermission;
  },

});
})();
