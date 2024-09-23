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

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './pin_keyboard.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {ConfigureResult, PinFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenProgress, recordLockScreenProgress} from './lock_screen_constants.js';
import {PinKeyboardElement} from './pin_keyboard.js';
import {getTemplate} from './setup_pin_keyboard.html.js';
import {fireAuthTokenInvalidEvent} from './utils.js';

/**
 * Keep in sync with the string keys provided by settings.
 */
export enum MessageType {
  TOO_SHORT = 'configurePinTooShort',
  TOO_LONG = 'configurePinTooLong',
  TOO_WEAK = 'configurePinWeakPin',
  CONTAINS_NONDIGIT = 'configurePinNondigit',
  MISMATCH = 'configurePinMismatched',
  INTERNAL_ERROR = 'internalError',
}

export enum ProblemType {
  WARNING = 'warning',
  ERROR = 'error',
}

const SetupPinKeyboardElementBase = I18nMixin(PolymerElement);

export interface SetupPinKeyboardElement {
  $: {
    pinKeyboard: PinKeyboardElement,
  };
}

export class SetupPinKeyboardElement extends SetupPinKeyboardElementBase {
  static get is() {
    return 'setup-pin-keyboard' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The token to be used to call into the PinFactorEditor mojo service.
       */
      authToken: String,

      /**
       * The current PIN keyboard value.
       */
      pinKeyboardValue_: {
        type: String,
        value: '',
      },

      /**
       * Stores the initial PIN value so it can be confirmed.
       */
      initialPin_: {
        type: String,
        value: '',
      },

      /**
       * The message ID of actual problem message to display.
       */
      problemMessageId_: {
        type: String,
        value: '',
      },

      /**
       * The additional parameters to format for the problem message string.
       */
      problemMessageParameters_: {
        type: String,
        value: '',
      },

      /**
       * The type of problem class to show (warning or error).
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
       */
      quickUnlockPrivate: Object,

      /**
       * |pinHasPassedMinimumLength_| tracks whether a user has passed the
       * minimum length threshold at least once, and all subsequent PIN too
       * short messages will be displayed as errors. They will be displayed as
       * warnings prior to this.
       */
      pinHasPassedMinimumLength_: {type: Boolean, value: false},

      /**
       * Enables pin placeholder.
       */
      enablePlaceholder: {
        type: Boolean,
        value: false,
      },

      isSetPinCallPending_: {
        notify: true,
        type: Boolean,
        value: false,
      },
    };
  }

  private pinKeyboardValue_: string;
  private initialPin_: string;
  private problemMessageId_: string;
  private problemMessageParameters_: string;
  private problemClass_: string|undefined;
  private pinHasPassedMinimumLength_: boolean;
  private isSetPinCallPending_: boolean;
  authToken: string|undefined;
  enableSubmit: boolean;
  writeUma: (progress: LockScreenProgress) => void;
  isConfirmStep: boolean;
  quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  enablePlaceholder: boolean;

  override focus(): void {
    this.$.pinKeyboard.focusInput();
  }

  override connectedCallback(): void {
    this.resetState();

    // Show the pin is too short error when first displaying the PIN dialog.
    this.problemClass_ = ProblemType.WARNING;
    chrome.quickUnlockPrivate.getCredentialRequirements(
        chrome.quickUnlockPrivate.QuickUnlockMode.PIN,
        this.processPinRequirements_.bind(this, MessageType.TOO_SHORT));
  }

  /**
   * Resets the element to the initial state.
   */
  resetState(): void {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.enableSubmit = false;
    this.isConfirmStep = false;
    this.pinHasPassedMinimumLength_ = false;
    this.hideProblem_();
    this.onPinChange_(
        new CustomEvent('pin-change', {detail: {pin: this.pinKeyboardValue_}}));
  }

  /**
   * Returns true if the PIN is ready to be changed to a new value.
   */
  private canSubmit_(): boolean {
    return this.initialPin_ === this.pinKeyboardValue_;
  }

  /**
   * Handles writing the appropriate message to |problemMessageId_| &&
   * |problemMessageParameters_|.
   * @param messageId
   * @param requirements
   *     The requirements received from getCredentialRequirements.
   */
  private processPinRequirements_(
      messageId: MessageType,
      requirements: chrome.quickUnlockPrivate.CredentialRequirements): void {
    let additionalInformation = '';
    switch (messageId) {
      case MessageType.TOO_SHORT:
        additionalInformation = requirements.minLength.toString();
        break;
      case MessageType.TOO_LONG:
        additionalInformation = (requirements.maxLength + 1).toString();
        break;
      case MessageType.TOO_WEAK:
      case MessageType.CONTAINS_NONDIGIT:
      case MessageType.MISMATCH:
      case MessageType.INTERNAL_ERROR:
        break;
      default:
        assertNotReached();
    }
    this.problemMessageId_ = messageId;
    this.problemMessageParameters_ = additionalInformation;
  }

  /**
   * Notify the user about a problem.
   */
  private showProblem_(messageId: MessageType, problemClass: ProblemType):
      void {
    this.quickUnlockPrivate.getCredentialRequirements(
        chrome.quickUnlockPrivate.QuickUnlockMode.PIN,
        this.processPinRequirements_.bind(this, messageId));
    this.problemClass_ = problemClass;
    this.enableSubmit = problemClass !== ProblemType.ERROR &&
        messageId !== MessageType.TOO_SHORT;
  }

  private hideProblem_(): void {
    this.problemMessageId_ = '';
    this.problemClass_ = '';
  }

  /**
   * Processes the message received from the quick unlock api and hides/shows
   * the problem based on the message.
   */
  private processPinProblems_(message:
                                  chrome.quickUnlockPrivate.CredentialCheck) {
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
        case chrome.quickUnlockPrivate.CredentialProblem.CONTAINS_NONDIGIT:
          this.showProblem_(MessageType.CONTAINS_NONDIGIT, ProblemType.ERROR);
          break;
        default:
          assertNotReached();
      }
    }
  }

  /**
   * @param e Custom event containing the new pin.
   */
  private onPinChange_(e: CustomEvent<{pin: string}>): void {
    const newPin = e.detail.pin;
    if (!this.isConfirmStep) {
      if (newPin) {
        this.quickUnlockPrivate.checkCredential(
            chrome.quickUnlockPrivate.QuickUnlockMode.PIN, newPin,
            this.processPinProblems_.bind(this));
      } else {
        this.enableSubmit = false;
        this.showProblem_(
            MessageType.TOO_SHORT,
            this.pinHasPassedMinimumLength_ ? ProblemType.ERROR :
                                              ProblemType.WARNING);
      }
      return;
    }

    this.hideProblem_();
    this.enableSubmit = newPin.length > 0;
  }

  private onPinSubmit_(): void {
    // Notify container object.
    this.dispatchEvent(new Event('pin-submit'));
  }

  /** This is called by container object when user initiated submit. */
  async doSubmit(): Promise<void> {
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
      recordLockScreenProgress(LockScreenProgress.ENTER_PIN);
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

    if (typeof this.authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    this.isSetPinCallPending_ = true;
    this.enableSubmit = false;
    const {result} = await PinFactorEditor.getRemote().setPin(
        this.authToken, this.pinKeyboardValue_);
    this.isSetPinCallPending_ = false;

    switch (result) {
      case ConfigureResult.kSuccess:
        break;
      case ConfigureResult.kInvalidTokenError:
        fireAuthTokenInvalidEvent(this);
        break;
      case ConfigureResult.kFatalError:
        console.error('Failed to update pin');
        this.showProblem_(MessageType.INTERNAL_ERROR, ProblemType.ERROR);
        // Enable submission again: We don't know why this failed, and perhaps
        // submitting again resolves the issue.
        this.enableSubmit = true;
        // Do not reset state, close the dialog or generate a set-pin-done
        // event -- this would lead the user to think that setting PIN was
        // successful when it has actually failed.
        return;
    }

    this.resetState();
    this.dispatchEvent(new Event('set-pin-done'));
    recordLockScreenProgress(LockScreenProgress.CONFIRM_PIN);
  }

  private hasError_(problemMessageId: string, problemClass: ProblemType):
      boolean {
    return !!problemMessageId && problemClass === ProblemType.ERROR;
  }

  /**
   * Format problem message
   */
  private formatProblemMessage_(
      locale: string, messageId: string|undefined,
      messageParameters: string): string {
    return messageId ? this.i18nDynamic(locale, messageId, messageParameters) :
                       '';
  }
}

customElements.define(SetupPinKeyboardElement.is, SetupPinKeyboardElement);
declare global {
  interface HTMLElementTagNameMap {
    [SetupPinKeyboardElement.is]: SetupPinKeyboardElement;
  }
}
