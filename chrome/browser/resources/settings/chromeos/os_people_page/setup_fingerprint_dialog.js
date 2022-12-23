// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cr_elements/cr_fingerprint/cr_fingerprint_progress_arc.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintResultType, FingerprintScan} from './fingerprint_browser_proxy.js';
import {getTemplate} from './setup_fingerprint_dialog.html.js';

/**
 * The steps in the fingerprint setup flow.
 * @enum {number}
 */
export const FingerprintSetupStep = {
  LOCATE_SCANNER: 1,  // The user needs to locate the scanner.
  MOVE_FINGER: 2,     // The user needs to move finger around the scanner.
  READY: 3,           // The scanner has read the fingerprint successfully.
};

/**
 * The amount of milliseconds after a successful but not completed scan before
 * a message shows up telling the user to scan their finger again.
 * @type {number}
 */
const SHOW_TAP_SENSOR_MESSAGE_DELAY_MS = 2000;

/**
 * The onboarding animation asset for dark mode.
 * @type {string}
 */
const ONBOARDING_ANIMATION_DARK = 'fingerprint_scanner_animation_dark.json';

/**
 * The onboarding animation asset for light mode.
 * @type {string}
 */
const ONBOARDING_ANIMATION_LIGHT = 'fingerprint_scanner_animation_light.json';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSetupFingerprintDialogElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsSetupFingerprintDialogElement extends
    SettingsSetupFingerprintDialogElementBase {
  static get is() {
    return 'settings-setup-fingerprint-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       * @type {!FingerprintSetupStep}
       * @private
       */
      step_: {type: Number, value: FingerprintSetupStep.LOCATE_SCANNER},

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
       * Whether the dialog is being rendered in dark mode.
       * @type {boolean}
       * @private
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * A message shows after the user has not scanned a finger during setup.
     * This is the set timeout id.
     * @type {number}
     * @private
     */
    this.tapSensorMessageTimeoutId_ = 0;

    /** @private {?FingerprintBrowserProxy} */
    this.browserProxy_ = FingerprintBrowserProxyImpl.getInstance();
  }


  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'on-fingerprint-scan-received', this.onScanReceived_.bind(this));
    this.addWebUIListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.$.arc.reset();
    this.browserProxy_.startEnroll(this.authToken);
    this.$.dialog.showModal();
  }

  /**
   * Closes the dialog.
   */
  close() {
    // Note: Reset resets |step_| back to the default, so handle anything that
    // checks |step_| before resetting.
    if (this.step_ !== FingerprintSetupStep.READY) {
      this.browserProxy_.cancelCurrentEnroll();
    }

    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    this.reset_();
  }

  /** private */
  clearSensorMessageTimeout_() {
    if (this.tapSensorMessageTimeoutId_ !== 0) {
      clearTimeout(this.tapSensorMessageTimeoutId_);
      this.tapSensorMessageTimeoutId_ = 0;
    }
  }

  /**
   * Resets the dialog to its start state. Call this when the dialog gets
   * closed.
   * @private
   */
  reset_() {
    this.step_ = FingerprintSetupStep.LOCATE_SCANNER;
    this.percentComplete_ = 0;
    this.clearSensorMessageTimeout_();
  }

  /**
   * Closes the dialog.
   * @private
   */
  onClose_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  /**
   * Advances steps, shows problems and animates the progress as needed based
   * on scan results.
   * @param {!FingerprintScan} scan
   * @private
   */
  onScanReceived_(scan) {
    switch (this.step_) {
      case FingerprintSetupStep.LOCATE_SCANNER:
        this.$.arc.reset();
        this.step_ = FingerprintSetupStep.MOVE_FINGER;
        this.percentComplete_ = scan.percentComplete;
        this.setProblem_(scan.result);
        break;
      case FingerprintSetupStep.MOVE_FINGER:
        if (scan.isComplete) {
          this.problemMessage_ = '';
          this.step_ = FingerprintSetupStep.READY;
          this.clearSensorMessageTimeout_();
          const event = new CustomEvent(
              'add-fingerprint', {bubbles: true, composed: true});
          this.dispatchEvent(event);
        } else {
          this.setProblem_(scan.result);
        }
        this.percentComplete_ = scan.percentComplete;
        break;
      case FingerprintSetupStep.READY:
        break;
      default:
        assertNotReached();
        break;
    }
  }

  /**
   * When the screen is getting locked during enrollment we close
   * the dialog to cancel the enrollment process and make the fingerprint
   * unlock available to the user.
   */
  onScreenLocked_(screenIsLocked) {
    if (screenIsLocked) {
      this.close();
    }
  }


  /**
   * Sets the instructions based on which phase of the fingerprint setup we
   * are on.
   * @param {!FingerprintSetupStep} step The current step the
   *     fingerprint setup is on.
   * @param {string} problemMessage Message for the scan result.
   * @private
   */
  getInstructionMessage_(step, problemMessage) {
    switch (step) {
      case FingerprintSetupStep.LOCATE_SCANNER:
        return this.i18n('configureFingerprintInstructionLocateScannerStep');
      case FingerprintSetupStep.MOVE_FINGER:
        return problemMessage;
      case FingerprintSetupStep.READY:
        return this.i18n('configureFingerprintInstructionReadyStep');
    }
    assertNotReached();
  }

  /**
   * Set the problem message based on the result from the fingerprint scanner.
   * @param {!FingerprintResultType} scanResult The result the
   *     fingerprint scanner gives.
   * @private
   */
  setProblem_(scanResult) {
    this.clearSensorMessageTimeout_();
    switch (scanResult) {
      case FingerprintResultType.SUCCESS:
        this.problemMessage_ = '';
        this.tapSensorMessageTimeoutId_ = setTimeout(() => {
          this.problemMessage_ = this.i18n('configureFingerprintLiftFinger');
        }, SHOW_TAP_SENSOR_MESSAGE_DELAY_MS);
        break;
      case FingerprintResultType.PARTIAL:
      case FingerprintResultType.INSUFFICIENT:
      case FingerprintResultType.SENSOR_DIRTY:
      case FingerprintResultType.TOO_SLOW:
      case FingerprintResultType.TOO_FAST:
        this.problemMessage_ = this.i18n('configureFingerprintTryAgain');
        break;
      case FingerprintResultType.IMMOBILE:
        this.problemMessage_ = this.i18n('configureFingerprintImmobile');
        break;
      default:
        assertNotReached();
        break;
    }
  }

  /**
   * Displays the text of the close button based on which phase of the
   * fingerprint setup we are on.
   * @param {!FingerprintSetupStep} step The current step the
   *     fingerprint setup is on.
   * @private
   */
  getCloseButtonText_(step) {
    if (step === FingerprintSetupStep.READY) {
      return this.i18n('done');
    }

    return this.i18n('cancel');
  }

  /**
   * @param {!FingerprintSetupStep} step
   * @private
   */
  getCloseButtonClass_(step) {
    if (step === FingerprintSetupStep.READY) {
      return 'action-button';
    }

    return 'cancel-button';
  }

  /**
   * @param {!FingerprintSetupStep} step
   * @param {boolean} allowAddAnotherFinger
   * @private
   */
  hideAddAnother_(step, allowAddAnotherFinger) {
    return step !== FingerprintSetupStep.READY || !allowAddAnotherFinger;
  }

  /**
   * Enrolls the finished fingerprint and sets the dialog back to step one to
   * prepare to enroll another fingerprint.
   * @private
   */
  onAddAnotherFingerprint_() {
    this.reset_();
    this.$.arc.reset();
    this.step_ = FingerprintSetupStep.MOVE_FINGER;
    this.browserProxy_.startEnroll(this.authToken);
    recordSettingChange();
  }

  /**
   * Whether scanner location should be shown at the current step.
   * @private
   */
  showScannerLocation_() {
    return this.step_ === FingerprintSetupStep.LOCATE_SCANNER;
  }

  /**
   * Whether fingerprint progress circle should be shown at the current step.
   * @private
   */
  showArc_() {
    return this.step_ === FingerprintSetupStep.MOVE_FINGER ||
        this.step_ === FingerprintSetupStep.READY;
  }

  /**
   * Observer for percentComplete_.
   * @private
   */
  onProgressChanged_(newValue, oldValue) {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      this.$.arc.reset();
      return;
    }

    this.$.arc.setProgress(oldValue, newValue, newValue === 100);
  }

  /**
   * Returns the URL for the asset that defines the onboarding animation for the
   * current fingerprint sensor location.
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? ONBOARDING_ANIMATION_DARK :
                                    ONBOARDING_ANIMATION_LIGHT;
  }
}

customElements.define(
    SettingsSetupFingerprintDialogElement.is,
    SettingsSetupFingerprintDialogElement);
