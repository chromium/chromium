// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Fingerprint
 * Enrollment screen.
 */

'use strict';

(function() {

/**
 * These values must be kept in sync with the values in
 * third_party/cros_system_api/dbus/service_constants.h.
 * @enum {number}
 */
var FingerprintResultType = {
  SUCCESS: 0,
  PARTIAL: 1,
  INSUFFICIENT: 2,
  SENSOR_DIRTY: 3,
  TOO_SLOW: 4,
  TOO_FAST: 5,
  IMMOBILE: 6,
};

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  START: 'start',
  PROGRESS: 'progress',
};

Polymer({
  is: 'fingerprint-setup-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'onEnrollScanDone',
    'enableAddAnotherFinger',
  ],

  UI_STEPS: UIState,

  properties: {
    /**
     * The percentage of completion that has been received during setup.
     * The value within [0, 100] represents the percent of enrollment
     * completion.
     * @type {number}
     */
    percentComplete_: {
      type: Number,
      value: 0,
      observer: 'onProgressChanged_',
    },

    /**
     * Is current finger enrollment complete?
     * @type {boolean}
     */
    complete_: {
      type: Boolean,
      value: false,
      computed: 'enrollIsComplete_(percentComplete_)',
    },

    /**
     * Can we add another finger?
     * @type {boolean}
     */
    canAddFinger: {
      type: Boolean,
      value: true,
    },

    /**
     * The result of fingerprint enrollment scan.
     * @type {FingerprintResultType}
     * @private
     */
    scanResult_: {
      type: Number,
      value: FingerprintResultType.SUCCESS,
    },

    /**
     * True if lottie animation file should be used instead of an illustration.
     * @type {boolean}
     * @private
     */
    shouldUseLottieAnimation_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('useLottieAnimationForFingerprint');
      },
      readOnly: true,
    }
  },

  ready() {
    this.initializeLoginScreen('FingerprintSetupScreen', {
      resetAllowed: false,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  defaultUIStep() {
    return UIState.START;
  },

  onBeforeShow() {
    this.setAnimationState_(true);
  },

  onBeforeHide() {
    this.setAnimationState_(false);
  },

  /**
   * Called when a fingerprint enroll scan result is received.
   * @param {FingerprintResultType} scanResult Result of the enroll scan.
   * @param {boolean} isComplete Whether fingerprint enrollment is complete.
   * @param {number} percentComplete Percentage of completion of the enrollment.
   */
  onEnrollScanDone(scanResult, isComplete, percentComplete) {
    this.setUIStep(UIState.PROGRESS);

    this.percentComplete_ = percentComplete;
    this.scanResult_ = scanResult;
  },

  /**
   * Enable/disable add another finger.
   * @param {boolean} enable True if add another fingerprint is enabled.
   */
  enableAddAnotherFinger(enable) {
    this.canAddFinger = enable;
  },

  /**
   * Check whether Add Another button should be shown.
   * @return {boolean}
   * @private
   */
  isAnotherButtonVisible_(percentComplete, canAddFinger) {
    return percentComplete >= 100 && canAddFinger;
  },

  /**
   * This is 'on-tap' event handler for 'Skip' button for 'START' step.
   * @private
   */
  onSkipOnStart_(e) {
    this.userActed('setup-skipped-on-start');
  },

  /**
   * This is 'on-tap' event handler for 'Skip' button for 'PROGRESS' step.
   * @private
   */
  onSkipInProgress_(e) {
    this.userActed('setup-skipped-in-flow');
  },

  /**
   * Enable/disable lottie animation.
   * @param {boolean} playing True if animation should be playing.
   */
  setAnimationState_(playing) {
    if (this.shouldUseLottieAnimation_) {
      const lottieElement = /** @type{CrLottieElement} */ (
          this.$.setupFingerprint.querySelector('#scannerLocationLottie'));
      lottieElement.setPlay(playing);
      /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc)
          .setPlay(playing);
    }
  },

  /**
   * This is 'on-tap' event handler for 'Done' button.
   * @private
   */
  onDone_(e) {
    this.userActed('setup-done');
  },

  /**
   * This is 'on-tap' event handler for 'Add another' button.
   * @private
   */
  onAddAnother_(e) {
    this.percentComplete_ = 0;
    this.userActed('add-another-finger');
  },

  /**
   * Check whether fingerprint enrollment is in progress.
   * @return {boolean}
   * @private
   */
  enrollIsComplete_(percent) {
    return percent >= 100;
  },

  /**
   * Check whether fingerprint scan problem is IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemImmobile_(scan_result) {
    return scan_result === FingerprintResultType.IMMOBILE;
  },

  /**
   * Check whether fingerprint scan problem is other than IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemOther_(scan_result) {
    return scan_result != FingerprintResultType.SUCCESS &&
        scan_result != FingerprintResultType.IMMOBILE;
  },

  /**
   * Observer for percentComplete_.
   * @private
   */
  onProgressChanged_(newValue, oldValue) {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc).reset();
      this.scanResult_ = FingerprintResultType.SUCCESS;
      return;
    }

    /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc)
        .setProgress(oldValue, newValue, newValue === 100);
  },
});
})();
