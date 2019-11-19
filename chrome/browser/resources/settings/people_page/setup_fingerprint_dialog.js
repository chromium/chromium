// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * The steps in the fingerprint setup flow.
 * @enum {number}
 */
settings.FingerprintSetupStep = {
  LOCATE_SCANNER: 1,  // The user needs to locate the scanner.
  MOVE_FINGER: 2,     // The user needs to move finger around the scanner.
  READY: 3            // The scanner has read the fingerprint successfully.
};

/**
 * Fingerprint sensor locations corresponding to the FingerprintLocation
 * enumerators in
 * /chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h
 * @enum {number}
 */
settings.FingerprintLocation = {
  TABLET_POWER_BUTTON: 0,
  KEYBOARD_TOP_RIGHT: 1,
  KEYBOARD_BOTTOM_RIGHT: 2,
};

(function() {

/**
 * The amount of milliseconds after a successful but not completed scan before a
 * message shows up telling the user to scan their finger again.
 * @type {number}
 */
const SHOW_TAP_SENSOR_MESSAGE_DELAY_MS = 2000;

Polymer({
  is: 'settings-setup-fingerprint-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Whether add another finger is allowed.
     * @type {boolean}
     */
    allowAddAnotherFinger: {
      type: Boolean,
      value: true,
    },

    /**
     * Authentication token provided by settings-fingerprint-list
     */
    authToken: {
      type: String,
      value: '',
    },
    /**
     * The problem message to display.
     * @private
     */
    problemMessage_: {
      type: String,
      value: '',
    },

    /**
     * The setup phase we are on.
     * @type {!settings.FingerprintSetupStep}
     * @private
     */
    step_: {type: Number, value: settings.FingerprintSetupStep.LOCATE_SCANNER},

    /**
     * The percentage of completion that has been received during setup.
     * This is used to approximate the progress of the setup.
     * The value within [0, 100] represents the percent of enrollment
     * completion.
     * @type {number}
     * @private
     */
    percentComplete_: {
      type: Number,
      value: 0,
      observer: 'onProgressChanged_',
    },

    /**
     * This is used to display right animation for fingerprint sensor.
     * @private {string}
     */
    fingerprintScannerAnimationClass_: {
      type: String,
      value: function() {
        if (!loadTimeData.getBoolean('fingerprintUnlockEnabled')) {
          return '';
        }
        const fingerprintLocation =
            loadTimeData.getInteger('fingerprintReaderLocation');
        switch (fingerprintLocation) {
          case settings.FingerprintLocation.TABLET_POWER_BUTTON:
            return '';
          case settings.FingerprintLocation.KEYBOARD_TOP_RIGHT:
            return 'fingerprint-scanner-laptop-top-right';
          case settings.FingerprintLocation.KEYBOARD_BOTTOM_RIGHT:
            return 'fingerprint-scanner-laptop-bottom-right';
        }
        assertNotReached();
      },
      readOnly: true,
    },

    /**
     * True lottie animation file should be used instead of a png animation
     * image sequence.
     * @private {boolean}
     */
    shouldUseLottieAnimation_: {
      type: Boolean,
      value: function() {
        if (!loadTimeData.getBoolean('fingerprintUnlockEnabled')) {
          return false;
        }

        const fingerprintLocation =
            loadTimeData.getInteger('fingerprintReaderLocation');
        const isTabletPowerButton =
            settings.FingerprintLocation.TABLET_POWER_BUTTON ==
            fingerprintLocation;
        return isTabletPowerButton;
      },
      readOnly: true,
    }
  },

  /**
   * A message shows after the user has not scanned a finger during setup. This
   * is the set timeout id.
   * @type {number}
   * @private
   */
  tapSensorMessageTimeoutId_: 0,

  /** @private {?settings.FingerprintBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'on-fingerprint-scan-received', this.onScanReceived_.bind(this));
    this.browserProxy_ = settings.FingerprintBrowserProxyImpl.getInstance();

    this.$.arc.reset();
    this.browserProxy_.startEnroll(this.authToken);
    this.$.dialog.showModal();
  },

  /**
   * Closes the dialog.
   */
  close: function() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    // Note: Reset resets |step_| back to the default, so handle anything that
    // checks |step_| before resetting.
    if (this.step_ != settings.FingerprintSetupStep.READY) {
      this.browserProxy_.cancelCurrentEnroll();
    }

    this.reset_();
  },

  /** private */
  clearSensorMessageTimeout_: function() {
    if (this.tapSensorMessageTimeoutId_ != 0) {
      clearTimeout(this.tapSensorMessageTimeoutId_);
      this.tapSensorMessageTimeoutId_ = 0;
    }
  },

  /**
   * Resets the dialog to its start state. Call this when the dialog gets
   * closed.
   * @private
   */
  reset_: function() {
    this.step_ = settings.FingerprintSetupStep.LOCATE_SCANNER;
    this.percentComplete_ = 0;
    this.clearSensorMessageTimeout_();
  },

  /**
   * Closes the dialog.
   * @private
   */
  onClose_: function() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },

  /**
   * Advances steps, shows problems and animates the progress as needed based on
   * scan results.
   * @param {!settings.FingerprintScan} scan
   * @private
   */
  onScanReceived_: function(scan) {
    switch (this.step_) {
      case settings.FingerprintSetupStep.LOCATE_SCANNER:
        this.$.arc.reset();
        this.step_ = settings.FingerprintSetupStep.MOVE_FINGER;
        this.percentComplete_ = scan.percentComplete;
        this.setProblem_(scan.result);
        break;
      case settings.FingerprintSetupStep.MOVE_FINGER:
        if (scan.isComplete) {
          this.problemMessage_ = '';
          this.step_ = settings.FingerprintSetupStep.READY;
          this.clearSensorMessageTimeout_();
          this.fire('add-fingerprint');
        } else {
          this.setProblem_(scan.result);
        }
        this.percentComplete_ = scan.percentComplete;
        break;
      case settings.FingerprintSetupStep.READY:
        break;
      default:
        assertNotReached();
        break;
    }
  },

  /**
   * Sets the instructions based on which phase of the fingerprint setup we are
   * on.
   * @param {!settings.FingerprintSetupStep} step The current step the
   *     fingerprint setup is on.
   * @param {string} problemMessage Message for the scan result.
   * @private
   */
  getInstructionMessage_: function(step, problemMessage) {
    switch (step) {
      case settings.FingerprintSetupStep.LOCATE_SCANNER:
        return this.i18n('configureFingerprintInstructionLocateScannerStep');
      case settings.FingerprintSetupStep.MOVE_FINGER:
        return problemMessage;
      case settings.FingerprintSetupStep.READY:
        return this.i18n('configureFingerprintInstructionReadyStep');
    }
    assertNotReached();
  },

  /**
   * Set the problem message based on the result from the fingerprint scanner.
   * @param {!settings.FingerprintResultType} scanResult The result the
   *     fingerprint scanner gives.
   * @private
   */
  setProblem_: function(scanResult) {
    this.clearSensorMessageTimeout_();
    switch (scanResult) {
      case settings.FingerprintResultType.SUCCESS:
        this.problemMessage_ = '';
        this.tapSensorMessageTimeoutId_ = setTimeout(() => {
          this.problemMessage_ = this.i18n('configureFingerprintLiftFinger');
        }, SHOW_TAP_SENSOR_MESSAGE_DELAY_MS);
        break;
      case settings.FingerprintResultType.PARTIAL:
      case settings.FingerprintResultType.INSUFFICIENT:
      case settings.FingerprintResultType.SENSOR_DIRTY:
      case settings.FingerprintResultType.TOO_SLOW:
      case settings.FingerprintResultType.TOO_FAST:
        this.problemMessage_ = this.i18n('configureFingerprintTryAgain');
        break;
      case settings.FingerprintResultType.IMMOBILE:
        this.problemMessage_ = this.i18n('configureFingerprintImmobile');
        break;
      default:
        assertNotReached();
        break;
    }
  },

  /**
   * Displays the text of the close button based on which phase of the
   * fingerprint setup we are on.
   * @param {!settings.FingerprintSetupStep} step The current step the
   *     fingerprint setup is on.
   * @private
   */
  getCloseButtonText_: function(step) {
    if (step == settings.FingerprintSetupStep.READY) {
      return this.i18n('done');
    }

    return this.i18n('cancel');
  },

  /**
   * @param {!settings.FingerprintSetupStep} step
   * @private
   */
  getCloseButtonClass_: function(step) {
    if (step == settings.FingerprintSetupStep.READY) {
      return 'action-button';
    }

    return 'cancel-button';
  },

  /**
   * @param {!settings.FingerprintSetupStep} step
   * @param {boolean} allowAddAnotherFinger
   * @private
   */
  hideAddAnother_: function(step, allowAddAnotherFinger) {
    return step != settings.FingerprintSetupStep.READY ||
        !allowAddAnotherFinger;
  },

  /**
   * Enrolls the finished fingerprint and sets the dialog back to step one to
   * prepare to enroll another fingerprint.
   * @private
   */
  onAddAnotherFingerprint_: function() {
    this.reset_();
    this.$.arc.reset();
    this.step_ = settings.FingerprintSetupStep.MOVE_FINGER;
    this.browserProxy_.startEnroll(this.authToken);
  },

  /**
   * Whether scanner location should be shown at the current step.
   * @private
   */
  showScannerLocation_: function() {
    return this.step_ == settings.FingerprintSetupStep.LOCATE_SCANNER;
  },

  /**
   * Whether fingerprint progress circle should be shown at the current step.
   * @private
   */
  showArc_: function() {
    return this.step_ == settings.FingerprintSetupStep.MOVE_FINGER ||
        this.step_ == settings.FingerprintSetupStep.READY;
  },

  /**
   * Observer for percentComplete_.
   * @private
   */
  onProgressChanged_: function(newValue, oldValue) {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      this.$.arc.reset();
      return;
    }

    this.$.arc.setProgress(oldValue, newValue, newValue === 100);
  },
});
})();
