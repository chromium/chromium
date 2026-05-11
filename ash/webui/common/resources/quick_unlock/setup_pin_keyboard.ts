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
import {AuthFactorConfig, ConfigureResult, LocalAuthFactorsComplexity, PinComplexity, PinFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenProgress, recordLockScreenProgress} from './lock_screen_constants.js';
import type {PinKeyboardElement} from './pin_keyboard.js';
import {getTemplate} from './setup_pin_keyboard.html.js';
import {fireAuthTokenInvalidEvent} from './utils.js';

import CredentialCheck = chrome.quickUnlockPrivate.CredentialCheck;
import CredentialProblem = chrome.quickUnlockPrivate.CredentialProblem;
import CredentialRequirements = chrome.quickUnlockPrivate.CredentialRequirements;
import QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;

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
  COMPLEXITY_NONE = 'configurePinComplexityErrorNone',
  COMPLEXITY_LOW = 'configurePinComplexityErrorLow',
  COMPLEXITY_MEDIUM = 'configurePinComplexityErrorMedium',
  COMPLEXITY_HIGH = 'configurePinComplexityErrorHigh',
}

export enum ProblemType {
  WARNING = 'warning',
  ERROR = 'error',
}

const ComplexityErrorMap: Record<
    Exclude<LocalAuthFactorsComplexity, LocalAuthFactorsComplexity.kUnset>,
    MessageType> = {
  [LocalAuthFactorsComplexity.kNone]: MessageType.COMPLEXITY_NONE,
  [LocalAuthFactorsComplexity.kLow]: MessageType.COMPLEXITY_LOW,
  [LocalAuthFactorsComplexity.kMedium]: MessageType.COMPLEXITY_MEDIUM,
  [LocalAuthFactorsComplexity.kHigh]: MessageType.COMPLEXITY_HIGH,
};

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
       * Auth token for making mojo calls into the backend.
       */
      authToken: {
        type: String,
        observer: 'fetchLocalAuthFactorsComplexity_',
      },

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
       * The current step/subpage we are on.
       * This is has upward data flow only.
       */
      isConfirmStep: {
        notify: true,
        type: Boolean,
        value: false,
      },

      // Whether the PIN keyboard is being used during ChromeOS recovery. In
      // that case, a different API should be used.
      useRecoveryModeApi: {
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

      /**
       * Enables the visibility icon for showing/hiding the PIN
       */
      enableVisibilityIcon: {
        type: Boolean,
        value: false,
      },

      isSetPinCallPending_: {
        notify: true,
        type: Boolean,
        value: false,
      },

      localAuthFactorsComplexity_: {
        type: Object,
        value: undefined,
        observer: 'updateDefaultMessage_',
      },
    };
  }

  authToken: string|undefined;
  enableSubmit: boolean;
  isConfirmStep: boolean;
  useRecoveryModeApi: boolean;
  quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  enablePlaceholder: boolean;
  enableVisibilityIcon: boolean;

  private pinKeyboardValue_: string;
  private initialPin_: string;
  private problemMessageId_: MessageType|'';
  private problemMessageParameters_: string;
  private problemClass_: ProblemType|''|undefined;
  private pinHasPassedMinimumLength_: boolean;
  private isSetPinCallPending_: boolean;
  private localAuthFactorsComplexity_: LocalAuthFactorsComplexity|undefined;
  private credentialRequirements_: CredentialRequirements|undefined;

  override focus(): void {
    this.$.pinKeyboard.focusInput();
  }

  override connectedCallback(): void {
    this.fetchCredentialRequirements_();
    this.resetState();
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
    this.useRecoveryModeApi = false;

    // Note: this.localAuthFactorsComplexity_ is NOT reset here.
    // This value represents the cached policy configuration for the current
    // user/device, which remains valid across multiple PIN entry attempts.
    // Keeping it avoids unnecessary re-fetching and prevents the legacy
    // 6-digit message "flash" during the fetch.
    this.updateDefaultMessage_();
  }

  /**
   * Notify the user about a problem.
   */
  private showProblem_(messageId: MessageType, problemClass: ProblemType):
      void {
    this.problemMessageId_ = messageId;
    this.problemClass_ = problemClass;

    let params = '';
    if (this.credentialRequirements_ !== undefined) {
      if (messageId === MessageType.TOO_SHORT) {
        params = this.credentialRequirements_.minLength.toString();
      } else if (messageId === MessageType.TOO_LONG) {
        params = (this.credentialRequirements_.maxLength + 1).toString();
      }
    }
    this.problemMessageParameters_ = params;
  }

  private hideProblem_(): void {
    this.problemMessageId_ = '';
    this.problemClass_ = '';
  }

  /**
   * Processes the message received from the quick unlock api and hides/shows
   * the problem based on the message.
   */
  private onQuickUnlockPrivateCheckCredential_({errors, warnings}:
                                                   CredentialCheck) {
    if (errors.length === 0) {
      // Enable submission since there are no errors.
      this.enableSubmit = true;
      this.pinHasPassedMinimumLength_ = true;

      if (warnings.length > 0) {
        assert(warnings[0] === CredentialProblem.TOO_WEAK);
        this.showProblem_(MessageType.TOO_WEAK, ProblemType.WARNING);
        return;
      }

      // No errors or warnings, hide the problem text.
      this.hideProblem_();
      return;
    }

    // Disable submission since we have an error.
    this.enableSubmit = false;
    if (errors[0] !== CredentialProblem.TOO_SHORT) {
      this.pinHasPassedMinimumLength_ = true;
    }

    switch (errors[0]) {
      case CredentialProblem.TOO_SHORT:
        this.showProblem_(
            MessageType.TOO_SHORT,
            this.pinHasPassedMinimumLength_ ? ProblemType.ERROR :
                                              ProblemType.WARNING);
        break;
      case CredentialProblem.TOO_LONG:
        this.showProblem_(MessageType.TOO_LONG, ProblemType.ERROR);
        break;
      case CredentialProblem.TOO_WEAK:
        this.showProblem_(MessageType.TOO_WEAK, ProblemType.ERROR);
        break;
      case CredentialProblem.CONTAINS_NONDIGIT:
        this.showProblem_(MessageType.CONTAINS_NONDIGIT, ProblemType.ERROR);
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Sends the PIN to the backend for validation.
   * Includes a safeguard to drop stale callbacks if the user's input changes.
   */
  private quickUnlockPrivateCheckCredential_(pin: string): void {
    this.quickUnlockPrivate.checkCredential(
        QuickUnlockMode.PIN, pin, (credentialCheck) => {
          // If the current input no longer matches the one we sent to the
          // backend, this is a stale callback. Ignore it to prevent UI
          // flakiness.
          if (this.pinKeyboardValue_ !== pin) {
            return;
          }
          this.onQuickUnlockPrivateCheckCredential_(credentialCheck);
        });
  }

  /**
   * @param e Custom event containing the new pin.
   */
  private onPinChange_(e: CustomEvent<{pin: string}>): void {
    const newPin = e.detail.pin;

    // Initial PIN setup.
    if (!this.isConfirmStep) {
      // Use the new flow if AuthFactorsComplexity policy is set.
      if (this.localAuthFactorsComplexity_ !==
          LocalAuthFactorsComplexity.kUnset) {
        this.checkPinComplexity_(newPin);
        return;
      }

      // Old quickUnlockPrivate flow.
      this.quickUnlockPrivateCheckCredential_(newPin);
      return;
    }

    // PIN confirmation.
    this.hideProblem_();
    this.enableSubmit = newPin.length > 0;
  }

  private onPinSubmit_(): void {
    // Notify container object.
    this.dispatchEvent(new Event('pin-submit'));
  }

  /** This is called by the container object when user initiates submit. */
  async doSubmit(): Promise<void> {
    if (!this.isConfirmStep) {
      this.handleInitialPinSubmit_();
    } else {
      this.handleConfirmPinSubmit_();
    }
  }

  private handleInitialPinSubmit_(): void {
    if (!this.enableSubmit) {
      return;
    }
    this.enableSubmit = false;
    this.initialPin_ = this.pinKeyboardValue_;
    // `isConfirmStep` MUST be set to true BEFORE clearing `pinKeyboardValue_`.
    // Clearing the PIN triggers a synchronous framework observer:
    // * `pinKeyboardValue_` is bound to `value` for pin-keyboard (.html file),
    // * which has an observer which fires a 'pin-change' event,
    // * which ends up calling `onPinChange_` here.
    // If `isConfirmStep` is still false when that observer fires, it sends a
    // request to the backend to validate an empty string. When that callback
    // eventually returns, it disables the submit button and causes tests to
    // timeout.
    this.isConfirmStep = true;
    this.pinKeyboardValue_ = '';
    this.$.pinKeyboard.resetPinVisibility();
    this.hideProblem_();
    this.$.pinKeyboard.focusInput();
    recordLockScreenProgress(LockScreenProgress.ENTER_PIN);
  }

  private async handleConfirmPinSubmit_(): Promise<void> {
    if (this.pinKeyboardValue_ !== this.initialPin_) {
      this.showProblem_(MessageType.MISMATCH, ProblemType.ERROR);
      this.enableSubmit = false;
      // Focus the PIN keyboard and highlight the entire PIN.
      this.$.pinKeyboard.focusInput(0, this.pinKeyboardValue_.length + 1);
      return;
    }

    await this.submitPinToBackend_();
  }

  private async submitPinToBackend_(): Promise<void> {
    if (typeof this.authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    this.enableSubmit = false;
    this.isSetPinCallPending_ = true;
    const {result} = await (this.useRecoveryModeApi ?
                                PinFactorEditor.getRemote().updatePin(
                                    this.authToken, this.pinKeyboardValue_) :
                                PinFactorEditor.getRemote().setPin(
                                    this.authToken, this.pinKeyboardValue_));
    this.isSetPinCallPending_ = false;

    this.handleBackendResult_(result);
  }

  private handleBackendResult_(result: ConfigureResult): void {
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

  private async checkPinComplexity_(pin: string): Promise<void> {
    if (typeof this.authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    let pinComplexity: PinComplexity;
    try {
      pinComplexity = await PinFactorEditor.getRemote().checkPinComplexity(
          this.authToken, pin);
    } catch (e) {
      switch (e) {
        case ConfigureResult.kInvalidTokenError:
          fireAuthTokenInvalidEvent(this);
          return;
        default:
          // The only error this API should return is `kInvalidTokenError`.
          assertNotReached();
      }
    }

    if (pinComplexity === PinComplexity.kOk) {
      this.enableSubmit = true;
      this.hideProblem_();
      return;
    }

    this.enableSubmit = false;
    let messageId = MessageType.TOO_WEAK;
    if (this.localAuthFactorsComplexity_ !== undefined &&
        this.localAuthFactorsComplexity_ !==
            LocalAuthFactorsComplexity.kUnset) {
      messageId = ComplexityErrorMap[this.localAuthFactorsComplexity_];
    }
    this.showProblem_(messageId, ProblemType.ERROR);
  }

  private async fetchLocalAuthFactorsComplexity_(): Promise<void> {
    if (typeof this.authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    try {
      const newValue =
          await AuthFactorConfig.getRemote().getLocalAuthFactorsComplexity(
              this.authToken!);
      if (newValue === this.localAuthFactorsComplexity_) {
        return;
      }
      this.localAuthFactorsComplexity_ = newValue;
    } catch (e) {
      console.error('Error calling fetchLocalAuthFactorsComplexity_:', e);
      this.localAuthFactorsComplexity_ = LocalAuthFactorsComplexity.kUnset;
      switch (e) {
        case ConfigureResult.kInvalidTokenError:
          fireAuthTokenInvalidEvent(this);
          break;
        default:
          // The only error this API should return is `kInvalidTokenError`.
          assertNotReached();
      }
    }
  }

  private updateDefaultMessage_(): void {
    // 1. Initial/fetching state - stay silent while we wait for the complexity
    // policy to avoid the legacy 6-digit message "flash".
    if (this.localAuthFactorsComplexity_ === undefined) {
      this.hideProblem_();
      return;
    }

    // 2. Complexity policy is in effect.
    if (this.localAuthFactorsComplexity_ !==
        LocalAuthFactorsComplexity.kUnset) {
      this.showProblem_(
          ComplexityErrorMap[this.localAuthFactorsComplexity_],
          ProblemType.WARNING);
      return;
    }

    // 3. Complexity policy is unset or fetch error (backend says "no policy"):
    // Fall back to the legacy 6-digit requirement.
    this.showProblem_(MessageType.TOO_SHORT, ProblemType.WARNING);
  }

  private shouldDisableKeyboard_(
      isSetPinCallPending: boolean,
      localAuthFactorsComplexity: LocalAuthFactorsComplexity|
      undefined): boolean {
    return isSetPinCallPending || localAuthFactorsComplexity === undefined;
  }

  /**
   * Fetches the PIN credential requirements and caches them locally.
   * Re-evaluates any pending problem messages once the fetch completes.
   *
   * Note: Caching these values means that if the policy updates while the
   * PIN dialog is already displayed, the messaging will continue to use the
   * old requirements. We accept this highly unlikely edge case.
   */
  private fetchCredentialRequirements_(): void {
    this.quickUnlockPrivate.getCredentialRequirements(
        QuickUnlockMode.PIN, (requirements: CredentialRequirements) => {
          this.credentialRequirements_ = requirements;

          // If we are currently showing an error/warning, re-trigger it now
          // that we have the requirements to format the string properly.
          if (this.problemMessageId_ && this.problemClass_) {
            this.showProblem_(this.problemMessageId_, this.problemClass_);
          }
        });
  }

  private hasError_(problemMessageId: string, problemClass: ProblemType):
      boolean {
    return !!problemMessageId && problemClass === ProblemType.ERROR;
  }

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
