// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying encryption migration screen.
 */

/**
 * Enum for the UI states corresponding to sub steps inside migration screen.
 * These values must be kept in sync with
 * EncryptionMigrationScreenHandler::UIState in C++ code.
 * @enum {number}
 */
var EncryptionMigrationUIState = {
  INITIAL: 0,
  READY: 1,
  MIGRATING: 2,
  MIGRATION_FAILED: 3,
  NOT_ENOUGH_SPACE: 4,
  MIGRATING_MINIMAL: 5
};

Polymer({
  is: 'encryption-migration',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Current UI state which corresponds to a sub step in migration process.
     * @type {EncryptionMigrationUIState}
     */
    uiState: {type: Number, value: 0},

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
   * Returns true if the migration is in initial state.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isInitial_: function(state) {
    return state == EncryptionMigrationUIState.INITIAL;
  },

  /**
   * Returns true if the migration is ready to start.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isReady_: function(state) {
    return state == EncryptionMigrationUIState.READY;
  },

  /**
   * Returns true if the migration is in progress.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigrating_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATING;
  },

  /**
   * Returns true if the migration failed.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigrationFailed_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATION_FAILED;
  },

  /**
   * Returns true if the available space is not enough to start migration.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isNotEnoughSpace_: function(state) {
    return state == EncryptionMigrationUIState.NOT_ENOUGH_SPACE;
  },

  /**
   * Returns true if we're in minimal migration mode.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigratingMinimal_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATING_MINIMAL;
  },

  /**
   * Returns true if the current migration progress is unknown.
   * @param {number} progress
   * @private
   */
  isProgressIndeterminate_: function(progress) {
    return progress < 0;
  },

  /**
   * Returns true if the 'Update' button should be disabled.
   * @param {boolean} isEnoughBattery
   * @param {boolean} isSkipped
   * @private
   */
  isUpdateDisabled_: function(isEnoughBattery, isSkipped) {
    return !isEnoughBattery || isSkipped;
  },

  /**
   * Returns true if the 'Skip' button on the initial screen should be hidden.
   * @return {boolean}
   */
  isSkipHidden_: function() {
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
  computeProgressLabel_: function(locale, progress) {
    return this.i18n('migrationProgressLabel', Math.floor(progress * 100));
  },

  /**
   * Computes the warning label when battery level is not enough.
   * @param {string} locale
   * @param {number} batteryPercent
   * @return {string}
   * @private
   */
  computeBatteryWarningLabel_: function(locale, batteryPercent) {
    return this.i18n('migrationBatteryWarningLabel', batteryPercent);
  },

  /**
   * Computes the label to show the necessary battery level for migration.
   * @param {string} locale
   * @param {number} necessaryBatteryPercent
   * @return {string}
   * @private
   */
  computeNecessaryBatteryLevelLabel_: function(
      locale, necessaryBatteryPercent) {
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
  computeAvailableSpaceLabel_: function(locale, availableSpaceInString) {
    return this.i18n('migrationAvailableSpaceLabel', availableSpaceInString);
  },

  /**
   * Computes the label to show the necessary space to start migration.
   * @param {string} locale
   * @param {string} necessarySpaceInString
   * @return {string}
   * @private
   */
  computeNecessarySpaceLabel_: function(locale, necessarySpaceInString) {
    return this.i18n('migrationNecessarySpaceLabel', necessarySpaceInString);
  },

  /**
   * Handles tap on UPGRADE button.
   * @private
   */
  onUpgrade_: function() {
    this.fire('upgrade');
  },

  /**
   * Handles tap on SKIP button.
   * @private
   */
  onSkip_: function() {
    this.isSkipped = true;
    this.fire('skip');
  },

  /**
   * Handles tap on RESTART button.
   * @private
   */
  onRestartOnLowStorage_: function() {
    this.fire('restart-on-low-storage');
  },

  /**
   * Handles tap on RESTART button on the migration failure screen.
   * @private
   */
  onRestartOnFailure_: function() {
    this.fire('restart-on-failure');
  },

  /**
   * Handles tap on REPORT AN ISSUE button.
   * @private
   */
  onReportAnIssue_: function() {
    this.fire('openFeedbackDialog');
  },
});
