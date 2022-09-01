// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Fingerprint
 * Enrollment screen.
 */

/* #js_imports_placeholder */

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
const FingerprintUIState = {
  START: 'start',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const FingerprintSetupBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   setupFingerprint:  OobeAdaptiveDialogElement,
 *   arc:  CrFingerprintProgressArcElement,
 * }}
 */
FingerprintSetupBase.$;

class FingerprintSetup extends FingerprintSetupBase {
  static get is() {
    return 'fingerprint-setup-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * The percentage of completion that has been received during setup.
       * The value within [0, 100] represents the percent of enrollment
       * completion.
       */
      percentComplete_: {
        type: Number,
        observer: 'onProgressChanged_',
      },

      /**
       * Is current finger enrollment complete?
       */
      complete_: {
        type: Boolean,
        computed: 'enrollIsComplete_(percentComplete_)',
      },

      /**
       * Can we add another finger?
       */
      canAddFinger: {
        type: Boolean,
      },

      /**
       * The result of fingerprint enrollment scan.
       * @private
       */
      scanResult_: {
        type: Number,
      },

      /**
       * Indicates whether user is a child account.
       */
      isChildAccount_: {
        type: Boolean,
      },

      /**
       * Indicates whether the fingerprint sensor location has a specific
       * aria-label.
       */
      hasAriaLabel_: {
        type: Boolean,
      },
    };
  }

  constructor() {
    super();
    this.UI_STEPS = FingerprintUIState;
    this.percentComplete_ = 0;
    this.complete_ = false;
    this.canAddFinger = true;
    this.scanResult_ = FingerprintResultType.SUCCESS;
    this.isChildAccount_ = false;
    this.hasAriaLabel_ = false;
  }

  /** @override */
  get EXTERNAL_API() {
    return ['onEnrollScanDone', 'enableAddAnotherFinger'];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('FingerprintSetupScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /** @override */
  defaultUIStep() {
    return FingerprintUIState.START;
  }

  onBeforeShow(data) {
    this.isChildAccount_ = data['isChildAccount'];
    this.hasAriaLabel_ = data['hasAriaLabel'];
    this.setAnimationState_(true);
  }

  onBeforeHide() {
    this.setAnimationState_(false);
  }

  /**
   * Called when a fingerprint enroll scan result is received.
   * @param {FingerprintResultType} scanResult Result of the enroll scan.
   * @param {boolean} isComplete Whether fingerprint enrollment is complete.
   * @param {number} percentComplete Percentage of completion of the enrollment.
   */
  onEnrollScanDone(scanResult, isComplete, percentComplete) {
    this.setUIStep(FingerprintUIState.PROGRESS);

    this.percentComplete_ = percentComplete;
    this.scanResult_ = scanResult;
  }

  /**
   * Enable/disable add another finger.
   * @param {boolean} enable True if add another fingerprint is enabled.
   */
  enableAddAnotherFinger(enable) {
    this.canAddFinger = enable;
  }

  /**
   * Check whether Add Another button should be shown.
   * @return {boolean}
   * @private
   */
  isAnotherButtonVisible_(percentComplete, canAddFinger) {
    return percentComplete >= 100 && canAddFinger;
  }

  /**
   * This is 'on-tap' event handler for 'Skip' button for 'START' step.
   * @private
   */
  onSkipOnStart_(e) {
    this.userActed('setup-skipped-on-start');
  }

  /**
   * This is 'on-tap' event handler for 'Skip' button for 'PROGRESS' step.
   * @private
   */
  onSkipInProgress_(e) {
    this.userActed('setup-skipped-in-flow');
  }

  /**
   * Enable/disable lottie animation.
   * @param {boolean} playing True if animation should be playing.
   */
  setAnimationState_(playing) {
    const lottieElement = /** @type{CrLottieElement} */ (
        this.$.setupFingerprint.querySelector('#scannerLocationLottie'));
    lottieElement.playing = playing;
    this.$.arc.setPlay(playing);
  }

  /**
   * This is 'on-tap' event handler for 'Done' button.
   * @private
   */
  onDone_(e) {
    this.userActed('setup-done');
  }

  /**
   * This is 'on-tap' event handler for 'Add another' button.
   * @private
   */
  onAddAnother_(e) {
    this.percentComplete_ = 0;
    this.userActed('add-another-finger');
  }

  /**
   * Check whether fingerprint enrollment is in progress.
   * @return {boolean}
   * @private
   */
  enrollIsComplete_(percent) {
    return percent >= 100;
  }

  /**
   * Check whether fingerprint scan problem is IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemImmobile_(scan_result) {
    return scan_result === FingerprintResultType.IMMOBILE;
  }

  /**
   * Check whether fingerprint scan problem is other than IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemOther_(scan_result) {
    return scan_result != FingerprintResultType.SUCCESS &&
        scan_result != FingerprintResultType.IMMOBILE;
  }

  /**
   * Observer for percentComplete_.
   * @private
   */
  onProgressChanged_(newValue, oldValue) {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      this.$.arc.reset();
      this.scanResult_ = FingerprintResultType.SUCCESS;
      return;
    }

    this.$.arc.setProgress(oldValue, newValue, newValue === 100);
  }

  /**
   * Returns the aria-label for the dialog.
   * New fingerprint positions do not require aria-labels since the exact
   * fingerprint sensor location is included in the subtitle, for these
   * locations use the screen title as the aria-label for the dialog.
   * @private
   */
  getAriaLabel_(locale, hasAriaLabel, isChildAccount) {
    if (hasAriaLabel) {
      return this.i18n('setupFingerprintScreenAriaLabel');
    }

    if (isChildAccount) {
      return this.i18n('setupFingerprintScreenTitleForChild');
    }
    return this.i18n('setupFingerprintScreenTitle');
  }
}

customElements.define(FingerprintSetup.is, FingerprintSetup);
