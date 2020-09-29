// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Demo Setup
 * screen.
 */

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  PROGRESS: 'progress',
  ERROR: 'error',
};

Polymer({
  is: 'demo-setup',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: ['setCurrentSetupStep', 'onSetupSucceeded', 'onSetupFailed'],

  properties: {
    /** Object mapping step strings to step indices */
    setupSteps_: {
      type: Object,
      value() {
        return /** @type {!Object} */ (loadTimeData.getValue('demoSetupSteps'));
      }
    },

    /** Which step index is currently running in Demo Mode setup. */
    currentStepIndex_: {
      type: Number,
      value: -1,
    },

    /** Error message displayed on demoSetupErrorDialog screen. */
    errorMessage_: {
      type: String,
      value: '',
    },

    /** Whether powerwash is required in case of a setup error. */
    isPowerwashRequired_: {
      type: Boolean,
      value: false,
    },

    /** Feature flag to display progress bar instead of spinner during setup. */
    showStepsInDemoModeSetup_: {
      type: Boolean,
      readonly: true,
      value() {
        return loadTimeData.getBoolean('showStepsInDemoModeSetup');
      }
    }
  },

  defaultUIStep() {
    return UIState.PROGRESS;
  },

  UI_STEPS: UIState,

  ready() {
    this.initializeLoginScreen('DemoSetupScreen', {
      resetAllowed: false,
    });
  },

  onBeforeShow() {
    this.reset();
  },

  /** Resets demo setup flow to the initial screen and starts setup. */
  reset() {
    this.setUIStep(UIState.PROGRESS);
    this.userActed('start-setup');
  },

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Called at the beginning of a setup step.
   * @param {string} currentStep The new step name.
   */
  setCurrentSetupStep(currentStep) {
    // If new step index not specified, remain unchanged.
    if (this.setupSteps_.hasOwnProperty(currentStep)) {
      this.currentStepIndex_ = this.setupSteps_[currentStep];
    }
  },

  /** Called when demo mode setup succeeded. */
  onSetupSucceeded() {
    this.errorMessage_ = '';
  },

  /**
   * Called when demo mode setup failed.
   * @param {string} message Error message to be displayed to the user.
   * @param {boolean} isPowerwashRequired Whether powerwash is required to
   *     recover from the error.
   */
  onSetupFailed(message, isPowerwashRequired) {
    this.errorMessage_ = message;
    this.isPowerwashRequired_ = isPowerwashRequired;
    this.setUIStep(UIState.ERROR);
  },

  /**
   * Retry button click handler.
   * @private
   */
  onRetryClicked_() {
    this.reset();
  },

  /**
   * Powerwash button click handler.
   * @private
   */
  onPowerwashClicked_() {
    this.userActed('powerwash');
  },

  /**
   * Close button click handler.
   * @private
   */
  onCloseClicked_() {
    // TODO(wzang): Remove this after crbug.com/900640 is fixed.
    if (this.isPowerwashRequired_)
      return;
    this.userActed('close-setup');
  },

  /**
   * Whether a given step should be rendered on the UI.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @private
   */
  shouldShowStep_(stepName, setupSteps) {
    return setupSteps.hasOwnProperty(stepName);
  },

  /**
   * Whether a given step is active.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @param {number} currentStepIndex
   * @private
   */
  stepIsActive_(stepName, setupSteps, currentStepIndex) {
    return currentStepIndex == setupSteps[stepName];
  },

  /**
   * Whether a given step is completed.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @param {number} currentStepIndex
   * @private
   */
  stepIsCompleted_(stepName, setupSteps, currentStepIndex) {
    return currentStepIndex > setupSteps[stepName];
  },

});
})();
