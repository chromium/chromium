// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Fingerprint
 * Enrollment screen.
 */

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

Polymer({
  is: 'fingerprint-setup',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

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
     * True if lottie animation should be used instead of animated PNGs.
     * @type {boolean}
     * @private
     */
    shouldUseLottieAnimation_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('useLottieAnimationForFingerprint');
      },
      readOnly: true,
    }
  },

  /*
   * Overridden from OobeDialogHostBehavior.
   * @override
   */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });

    this.showScreen_('setupFingerprint');
    chrome.send('startEnroll');
  },

  focus: function() {
    let activeScreen = this.getActiveScreen_();
    if (activeScreen)
      activeScreen.focus();
  },

  /**
   * Called when a fingerprint enroll scan result is received.
   * @param {FingerprintResultType} scanResult Result of the enroll scan.
   * @param {boolean} isComplete Whether fingerprint enrollment is complete.
   * @param {number} percentComplete Percentage of completion of the enrollment.
   */
  onEnrollScanDone: function(scanResult, isComplete, percentComplete) {
    // First tap on the sensor to start fingerprint enrollment.
    if (this.getActiveScreen_() === this.$.placeFinger ||
        this.getActiveScreen_() === this.$.setupFingerprint) {
      this.showScreen_('startFingerprintEnroll');
    }

    this.percentComplete_ = percentComplete;
    this.scanResult_ = scanResult;
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideAllScreens_: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens)
      screen.hidden = true;
  },

  /**
   * Returns active screen or null if none.
   * @private
   */
  getActiveScreen_: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens) {
      if (!screen.hidden)
        return screen;
    }
    return null;
  },

  /**
   * Shows given screen.
   * @param id String Screen ID.
   * @private
   */
  showScreen_: function(id) {
    this.hideAllScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
    screen.focus();

    // Reset enrollment progress when enrollment screen is shown.
    if (id === 'startFingerprintEnroll')
      this.percentComplete_ = 0;
  },

  /**
   * Check whether Add Another button should be shown.
   * @return {boolean}
   * @private
   */
  isAnotherButtonVisible_: function(percentComplete, canAddFinger) {
    return percentComplete >= 100 && canAddFinger;
  },

  /**
   * This is 'on-tap' event handler for 'Skip' and 'Do it later' button.
   * @private
   */
  onFingerprintSetupSkipped_: function(e) {
    chrome.send(
        'login.FingerprintSetupScreen.userActed', ['fingerprint-setup-done']);
  },

  /**
   * This is 'on-tap' event handler for 'showSensorLocationButton' button.
   * @private
   */
  onContinueToSensorLocationScreen_: function(e) {
    this.showScreen_('placeFinger');

    if (this.shouldUseLottieAnimation_) {
      const placeFingerScreen = this.getActiveScreen_();
      let lottieElement = /** @type{CrLottieElement} */ (
          placeFingerScreen.querySelector('#scannerLocationLottie'));
      lottieElement.setPlay(true);
    }
  },

  /**
   * This is 'on-tap' event handler for 'Done' button.
   * @private
   */
  onFingerprintSetupDone_: function(e) {
    chrome.send(
        'login.FingerprintSetupScreen.userActed', ['fingerprint-setup-done']);
  },

  /**
   * This is 'on-tap' event handler for 'Add another' button.
   * @private
   */
  onFingerprintAddAnother_: function(e) {
    this.percentComplete_ = 0;
    chrome.send('startEnroll');
  },

  /**
   * Check whether fingerprint enrollment is in progress.
   * @return {boolean}
   * @private
   */
  enrollInProgress_: function() {
    return this.percentComplete_ < 100;
  },

  /**
   * Check whether fingerprint scan problem is IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemImmobile_: function(scan_result) {
    return scan_result === FingerprintResultType.IMMOBILE;
  },

  /**
   * Check whether fingerprint scan problem is other than IMMOBILE.
   * @return {boolean}
   * @private
   */
  isProblemOther_: function(scan_result) {
    return scan_result != FingerprintResultType.SUCCESS &&
        scan_result != FingerprintResultType.IMMOBILE;
  },

  /**
   * Observer for percentComplete_.
   * @private
   */
  onProgressChanged_: function(newValue, oldValue) {
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
