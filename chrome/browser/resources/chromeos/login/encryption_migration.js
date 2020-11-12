// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying encryption migration screen.
 */

/**
 * Enum for the UI states corresponding to sub steps inside migration screen.
 * These values must be kept in sync with
 * EncryptionMigrationScreenView::UIState in C++ code and the order of the
 * enum must be the same.
 * @enum {string}
 */
var EncryptionMigrationUIState = {
  INITIAL: 'initial',
  READY: 'ready',
  MIGRATING: 'migrating',
  MIGRATION_FAILED: 'migration-failed',
  NOT_ENOUGH_SPACE: 'not-enough-space',
};

Polymer({
  is: 'encryption-migration-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'setUIState',
    'setMigrationProgress',
    'setIsResuming',
    'setBatteryState',
    'setNecessaryBatteryPercent',
    'setSpaceInfoInString',
  ],

  properties: {
    /**
     * Current migration progress in range [0, 1]. Negative value means that
     * the progress is unknown.
     */
    progress: {type: Number, value: -1},

    /**
     * Whether the current migration is resuming the previous one.
     */
    isResuming: {type: Boolean, value: false},

    /**
     * Battery level.
     */
    batteryPercent: {type: Number, value: 0},

    /**
     * Necessary battery level to start migration in percent.
     */
    necessaryBatteryPercent: {type: Number, value: 0},

    /**
     * True if the battery level is enough to start migration.
     */
    isEnoughBattery: {type: Boolean, value: true},

    /**
     * True if the device is charging.
     */
    isCharging: {type: Boolean, value: false},

    /**
     * True if the migration was skipped.
     */
    isSkipped: {type: Boolean, value: false},

    /**
     * Formatted string of the current available space size.
     */
    availableSpaceInString: {type: String, value: ''},

    /**
     * Formatted string of the necessary space size for migration.
     */
    necessarySpaceInString: {type: String, value: ''},
  },

  /**
   * Ignore any accelerators the user presses on this screen.
   */
  ignoreAccelerators: true,

  ready() {
    this.initializeLoginScreen('EncryptionMigrationScreen', {
      resetAllowed: false,
    });
  },

  defaultUIStep() {
    return EncryptionMigrationUIState.INITIAL;
  },

  UI_STEPS: EncryptionMigrationUIState,

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.MIGRATION;
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Updates the migration screen by specifying a state which corresponds
   * to a sub step in the migration process.
   * @param {number} state The UI state to identify a sub step in migration.
   */
  setUIState(state) {
    this.setUIStep(Object.values(EncryptionMigrationUIState)[state]);
  },

  /**
   * Updates the migration progress.
   * @param {number} progress The progress of migration in range [0, 1].
   */
  setMigrationProgress(progress) {
    this.progress = progress;
  },

  /**
   * Updates the migration screen based on whether the migration process
   * is resuming the previous one.
   * @param {boolean} isResuming
   */
  setIsResuming(isResuming) {
    this.isResuming = isResuming;
  },

  /**
   * Updates battery level of the device.
   * @param {number} batteryPercent Battery level in percent.
   * @param {boolean} isEnoughBattery True if the battery is enough.
   * @param {boolean} isCharging True if the device is connected to power.
   */
  setBatteryState(batteryPercent, isEnoughBattery, isCharging) {
    this.batteryPercent = Math.floor(batteryPercent);
    this.isEnoughBattery = isEnoughBattery;
    this.isCharging = isCharging;
  },

  /**
   * Update the necessary battery percent to start migration in the UI.
   * @param {number} necessaryBatteryPercent Necessary battery level.
   */
  setNecessaryBatteryPercent(necessaryBatteryPercent) {
    this.necessaryBatteryPercent = necessaryBatteryPercent;
  },

  /**
   * Updates the string representation of available space size and necessary
   * space size.
   * @param {string} availableSpaceSize
   * @param {string} necessarySpaceSize
   */
  setSpaceInfoInString(availableSpaceSize, necessarySpaceSize) {
    this.availableSpaceInString = availableSpaceSize;
    this.necessarySpaceInString = necessarySpaceSize;
  },

  /**
   * Returns true if the current migration progress is unknown.
   * @param {number} progress
   * @private
   */
  isProgressIndeterminate_(progress) {
    return progress < 0;
  },

  /**
   * Returns true if the 'Update' button should be disabled.
   * @param {boolean} isEnoughBattery
   * @param {boolean} isSkipped
   * @private
   */
  isUpdateDisabled_(isEnoughBattery, isSkipped) {
    return !isEnoughBattery || isSkipped;
  },

  /**
   * Returns true if the 'Skip' button on the initial screen should be hidden.
   * @return {boolean}
   */
  isSkipHidden_() {
    // TODO(fukino): Instead of checking the board name here to behave
    // differently, it's recommended to add a command-line flag to Chrome and
    // make session_manager pass it based on a feature-based USE flag which is
    // set in the appropriate board overlays.
    // https://goo.gl/BbBkzg.
    return this.i18n('migrationBoardName').startsWith('kevin');
  },

  /**
   * Computes the label shown under progress bar.
   * @param {string} locale
   * @param {number} progress
   * @return {string}
   * @private
   */
  computeProgressLabel_(locale, progress) {
    return this.i18n('migrationProgressLabel', Math.floor(progress * 100));
  },

  /**
   * Computes the warning label when battery level is not enough.
   * @param {string} locale
   * @param {number} batteryPercent
   * @return {string}
   * @private
   */
  computeBatteryWarningLabel_(locale, batteryPercent) {
    return this.i18n('migrationBatteryWarningLabel', batteryPercent);
  },

  /**
   * Computes the label to show the necessary battery level for migration.
   * @param {string} locale
   * @param {number} necessaryBatteryPercent
   * @return {string}
   * @private
   */
  computeNecessaryBatteryLevelLabel_(locale, necessaryBatteryPercent) {
    return this.i18n(
        'migrationNecessaryBatteryLevelLabel', necessaryBatteryPercent);
  },

  /**
   * Computes the label to show the current available space.
   * @param {string} locale
   * @param {string} availableSpaceInString
   * @return {string}
   * @private
   */
  computeAvailableSpaceLabel_(locale, availableSpaceInString) {
    return this.i18n('migrationAvailableSpaceLabel', availableSpaceInString);
  },

  /**
   * Computes the label to show the necessary space to start migration.
   * @param {string} locale
   * @param {string} necessarySpaceInString
   * @return {string}
   * @private
   */
  computeNecessarySpaceLabel_(locale, necessarySpaceInString) {
    return this.i18n('migrationNecessarySpaceLabel', necessarySpaceInString);
  },

  /**
   * Handles tap on UPGRADE button.
   * @private
   */
  onUpgrade_() {
    // TODO(crbug.com/1133705) Move the logic from handler to screen object and
    // use userActed call.
    this.userActed('startMigration');
  },

  /**
   * Handles tap on SKIP button.
   * @private
   */
  onSkip_() {
    this.isSkipped = true;
    this.userActed('skipMigration');
  },

  /**
   * Handles tap on RESTART button.
   * @private
   */
  onRestartOnLowStorage_() {
    this.userActed('requestRestartOnLowStorage');
  },

  /**
   * Handles tap on RESTART button on the migration failure screen.
   * @private
   */
  onRestartOnFailure_() {
    this.userActed('requestRestartOnFailure');
  },

  /**
   * Handles tap on REPORT AN ISSUE button.
   * @private
   */
  onReportAnIssue_() {
    this.userActed('openFeedbackDialog');
  },
});
