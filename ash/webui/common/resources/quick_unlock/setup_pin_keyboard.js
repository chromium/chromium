// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'setup-pin-keyboard' is the keyboard/input field for choosing a PIN.
 *
 * See usage documentation in setup_pin_keyboard.html.
 *
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './pin_keyboard.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenProgress} from './lock_screen_constants.js';
import {getTemplate} from './setup_pin_keyboard.html.js';

/**
 * Keep in sync with the string keys provided by settings.
 * @enum {string}
 */
export const MessageType = {
  TOO_SHORT: 'configurePinTooShort',
  TOO_LONG: 'configurePinTooLong',
  TOO_WEAK: 'configurePinWeakPin',
  MISMATCH: 'configurePinMismatched',
  INTERNAL_ERROR: 'internalError',
};

/** @enum {string} */
export const ProblemType = {
  WARNING: 'warning',
  ERROR: 'error',
};

Polymer({
  _template: getTemplate(),
  is: 'setup-pin-keyboard',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Reflects property set in password_prompt_dialog.js.
     * @type {?Object}
     */
    setModes: {
      type: Object,
      notify: true,
    },

    /**
     * The current PIN keyboard value.
     * @private
     */
    pinKeyboardValue_: String,

    /**
     * Stores the initial PIN value so it can be confirmed.
     * @private
     */
    initialPin_: String,

    /**
     * The message ID of actual problem message to display.
     * @private
     */
    problemMessageId_: {
      type: String,
      value: '',
    },

    /**
     * The additional parameters to format for the problem message string.
     * @private
     */
    problemMessageParameters_: {
      type: String,
      value: '',
    },

    /**
     * The type of problem class to show (warning or error).
     * @private
     */
    problemClass_: String,

    /**
     * Should the step-specific submit button be displayed?
     * This has upward data flow only.
     */
    enableSubmit: {
      notify: true,
      type: Boolean,
      value: false,
    },

    /**
     * writeUma is a function that handles writing uma stats.
     *
     * @type {function(LockScreenProgress)}
     */
    writeUma: {
      type: Object,
      value() {
        return function() {};
      },
    },

    /**
     * The current step/subpage we are on.
     * This is has upward data flow only.
     */
    isConfirmStep: {
      notify: true,
      type: Boolean,
      value: false,
    },

    /**
     * Interface for chrome.quickUnlockPrivate calls.
     * @type {QuickUnlockPrivate}
     */
    quickUnlockPrivate: Object,

    /**
     * |pinHasPassedMinimumLength_| tracks whether a user has passed the minimum
     * length threshold at least once, and all subsequent PIN too short messages
     * will be displayed as errors. They will be displayed as warnings prior to
     * this.
     * @private
     */
    pinHasPassedMinimumLength_: {type: Boolean, value: false},

    /**
     * Enables pin placeholder.
     */
    enablePlaceholder: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isSetModesCallPending_: {
      notify: true,
      type: Boolean,
      value: false,
    },
  },

  focus() {
    this.$.pinKeyboard.focusInput();
  },

  /** @override */
  attached() {
    this.resetState();

    // Show the pin is too short error when first displaying the PIN dialog.
    this.problemClass_ = ProblemType.WARNING;
    this.quickUnlockPrivate.getCredentialRequirements(
        chrome.quickUnlockPrivate.QuickUnlockMode.PIN,
        this.processPinRequirements_.bind(this, MessageType.TOO_SHORT));
  },

  /**
   * Resets the element to the initial state.
   */
  resetState() {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.enableSubmit = false;
    this.isConfirmStep = false;
    this.pinHasPassedMinimumLength_ = false;
    this.hideProblem_();
    this.onPinChange_(
        new CustomEvent('pin-change', {detail: {pin: this.pinKeyboardValue_}}));
  },

  /**
   * Returns true if the PIN is ready to be changed to a new value.
   * @private
   * @return {boolean}
   */
  canSubmit_() {
    return this.initialPin_ === this.pinKeyboardValue_;
  },

  /**
   * Handles writing the appropriate message to |problemMessageId_| &&
   * |problemMessageParameters_|.
   * @private
   * @param {string} messageId
   * @param {chrome.quickUnlockPrivate.CredentialRequirements} requirements
   *     The requirements received from getCredentialRequirements.
   */
  processPinRequirements_(messageId, requirements) {
    let additionalInformation = '';
    switch (messageId) {
      case MessageType.TOO_SHORT:
        additionalInformation = requirements.minLength.toString();
        break;
      case MessageType.TOO_LONG:
        additionalInformation = (requirements.maxLength + 1).toString();
        break;
      case MessageType.TOO_WEAK:
      case MessageType.MISMATCH:
      case MessageType.INTERNAL_ERROR:
        break;
      default:
        assertNotReached();
        break;
    }
    this.problemMessageId_ = messageId;
    this.problemMessageParameters_ = additionalInformation;
  },

  /**
   * Notify the user about a problem.
   * @private
   * @param {string} messageId
   * @param {string} problemClass
   */
  showProblem_(messageId, problemClass) {
    this.quickUnlockPrivate.getCredentialRequirements(
        chrome.quickUnlockPrivate.QuickUnlockMode.PIN,
        this.processPinRequirements_.bind(this, messageId));
    this.problemClass_ = problemClass;
    this.enableSubmit = problemClass !== ProblemType.ERROR &&
        messageId !== MessageType.TOO_SHORT;
  },

  /** @private */
  hideProblem_() {
    this.problemMessageId_ = '';
    this.problemClass_ = '';
  },

  /**
   * Processes the message received from the quick unlock api and hides/shows
   * the problem based on the message.
   * @private
   * @param {chrome.quickUnlockPrivate.CredentialCheck} message The message
   *     received from checkCredential.
   */
  processPinProblems_(message) {
    if (!message.errors.length && !message.warnings.length) {
      this.hideProblem_();
      this.enableSubmit = true;
      this.pinHasPassedMinimumLength_ = true;
      return;
    }

    if (!message.errors.length ||
        message.errors[0] !==
            chrome.quickUnlockPrivate.CredentialProblem.TOO_SHORT) {
      this.pinHasPassedMinimumLength_ = true;
    }

    if (message.warnings.length) {
      assert(
          message.warnings[0] ===
          chrome.quickUnlockPrivate.CredentialProblem.TOO_WEAK);
      this.showProblem_(MessageType.TOO_WEAK, ProblemType.WARNING);
    }

    if (message.errors.length) {
      switch (message.errors[0]) {
        case chrome.quickUnlockPrivate.CredentialProblem.TOO_SHORT:
          this.showProblem_(
              MessageType.TOO_SHORT,
              this.pinHasPassedMinimumLength_ ? ProblemType.ERROR :
                                                ProblemType.WARNING);
          break;
        case chrome.quickUnlockPrivate.CredentialProblem.TOO_LONG:
          this.showProblem_(MessageType.TOO_LONG, ProblemType.ERROR);
          break;
        case chrome.quickUnlockPrivate.CredentialProblem.TOO_WEAK:
          this.showProblem_(MessageType.TOO_WEAK, ProblemType.ERROR);
          break;
        default:
          assertNotReached();
          break;
      }
    }
  },

  /**
   * @param {!CustomEvent<{pin: string}>} e Custom event containing the new pin.
   * @private
   */
  onPinChange_(e) {
    const newPin = e.detail.pin;
    if (!this.isConfirmStep) {
      if (newPin) {
        this.quickUnlockPrivate.checkCredential(
            chrome.quickUnlockPrivate.QuickUnlockMode.PIN, newPin,
            this.processPinProblems_.bind(this));
      } else {
        this.enableSubmit = false;
      }
      return;
    }

    this.hideProblem_();
    this.enableSubmit = newPin.length > 0;
  },

  /** @private */
  onPinSubmit_() {
    // Notify container object.
    this.fire('pin-submit');
  },

  /**
   * This is callback for quickUnlockPrivate.QuickUnlockMode.PIN API.
   *
   * @private
   * @param {boolean} didSet
   */
  onSetModesCompleted_(didSet) {
    this.isSetModesCallPending_ = false;
    if (!didSet) {
      console.error('Failed to update pin');
      this.showProblem_(MessageType.INTERNAL_ERROR, ProblemType.ERROR);
      this.enableSubmit = true;
      return;
    }

    this.resetState();
    this.fire('set-pin-done');
  },

  /** This is called by container object when user initiated submit. */
  doSubmit() {
    if (!this.isConfirmStep) {
      if (!this.enableSubmit) {
        return;
      }
      this.initialPin_ = this.pinKeyboardValue_;
      this.pinKeyboardValue_ = '';
      this.isConfirmStep = true;
      this.onPinChange_(new CustomEvent(
          'pin-change', {detail: {pin: this.pinKeyboardValue_}}));
      this.$.pinKeyboard.focusInput();
      this.writeUma(LockScreenProgress.ENTER_PIN);
      return;
    }
    // onPinSubmit gets called if the user hits enter on the PIN keyboard.
    // The PIN is not guaranteed to be valid in that case.
    if (!this.canSubmit_()) {
      this.showProblem_(MessageType.MISMATCH, ProblemType.ERROR);
      this.enableSubmit = false;
      // Focus the PIN keyboard and highlight the entire PIN.
      this.$.pinKeyboard.focusInput(0, this.pinKeyboardValue_.length + 1);
      return;
    }

    assert(this.setModes);
    this.isSetModesCallPending_ = true;
    this.enableSubmit = false;
    this.setModes.call(
        null, [chrome.quickUnlockPrivate.QuickUnlockMode.PIN],
        [this.pinKeyboardValue_], this.onSetModesCompleted_.bind(this));
    this.writeUma(LockScreenProgress.CONFIRM_PIN);
  },

  /**
   * @private
   * @param {string} problemMessageId
   * @param {string} problemClass
   * @return {boolean}
   */
  hasError_(problemMessageId, problemClass) {
    return !!problemMessageId && problemClass === ProblemType.ERROR;
  },

  /**
   * Formar problem message
   * @private
   * @param {string} locale  i18n locale data
   * @param {string} messageId
   * @param {string} messageParameters
   * @return {string}
   */
  formatProblemMessage_(locale, messageId, messageParameters) {
    return messageId ? this.i18nDynamic(locale, messageId, messageParameters) :
                       '';
  },
});
