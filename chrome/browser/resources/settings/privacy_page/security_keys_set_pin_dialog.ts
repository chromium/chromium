// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-set-pin-dialog' is a dialog for
 * setting and changing security key PINs.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SecurityKeysPinBrowserProxy} from './security_keys_browser_proxy.js';
import {SecurityKeysPinBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {getTemplate} from './security_keys_set_pin_dialog.html.js';

export enum SetPinDialogPage {
  INITIAL = 'initial',
  NO_PIN_SUPPORT = 'noPINSupport',
  REINSERT = 'reinsert',
  LOCKED = 'locked',
  ERROR = 'error',
  PIN_PROMPT = 'pinPrompt',
  SUCCESS = 'success',
}

export interface SettingsSecurityKeysSetPinDialogElement {
  $: {
    closeButton: CrButtonElement,
    confirmPIN: CrInputElement,
    currentPIN: CrInputElement,
    currentPINEntry: HTMLElement,
    dialog: CrDialogElement,
    error: HTMLElement,
    newPIN: CrInputElement,
    pinSubmit: CrButtonElement,
  };
}

const SettingsSecurityKeysSetPinDialogElementBase = I18nMixin(PolymerElement);

export class SettingsSecurityKeysSetPinDialogElement extends
    SettingsSecurityKeysSetPinDialogElementBase {
  static get is() {
    return 'settings-security-keys-set-pin-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the value of the current PIN textbox is a valid PIN or not.
       */
      currentPINValid_: Boolean,

      newPINValid_: Boolean,
      confirmPINValid_: Boolean,

      /**
       * Whether the dialog is in a state where the Set PIN button should be
       * enabled. Read by Polymer.
       */
      setPINButtonValid_: {
        type: Boolean,
        value: false,
      },

      /**
       * The value of the new PIN textbox. Read/write by Polymer.
       */
      newPIN_: {
        type: String,
        value: '',
      },

      confirmPIN_: {
        type: String,
        value: '',
      },

      currentPIN_: {
        type: String,
        value: '',
      },

      /**
       * The minimum length for the currently set PIN.
       */
      currentMinPinLength_: Number,

      /**
       * The minimum length to set a new PIN.
       */
      newMinPinLength_: {
        type: Number,
        observer: 'newMinPinLengthChanged_',
      },

      /**
       * The number of PIN attempts remaining.
       */
      retries_: Number,

      /**
       * A CTAP error code when we don't recognise the specific error. Read by
       * Polymer.
       */
      errorCode_: Number,

      /**
       * Whether an entry for the current PIN should be displayed. (If no PIN
       * has been set then it won't be shown.)
       */
      showCurrentEntry_: {
        type: Boolean,
        value: false,
      },

      /**
       * Error string to display under the current PIN entry, or empty.
       */
      currentPINError_: {
        type: String,
        value: '',
      },

      /**
       * Error string to display under the new PIN entry, or empty.
       */
      newPINError_: {
        type: String,
        value: '',
      },

      /**
       * Error string to display under the confirmation PIN entry, or empty.
       */
      confirmPINError_: {
        type: String,
        value: '',
      },

      /**
       * Whether the dialog process has completed, successfully or otherwise.
       */
      complete_: {
        type: Boolean,
        value: false,
      },

      /**
       * The id of an element on the page that is currently shown.
       */
      shown_: {
        type: String,
        value: SetPinDialogPage.INITIAL,
      },

      /**
       * Whether the contents of the PIN entries are visible, or are displayed
       * like passwords.
       */
      pinsVisible_: {
        type: Boolean,
        value: false,
      },

      title_: String,
      newPINDialogDescription_: String,
    };
  }

  private currentPINValid_: boolean;
  private newPINValid_: boolean;
  private confirmPINValid_: boolean;
  private setPINButtonValid_: boolean;
  private newPIN_: string;
  private confirmPIN_: string;
  private currentPIN_: string;
  private currentMinPinLength_?: number;
  private newMinPinLength_?: number;
  private retries_?: number;
  private errorCode_?: number;
  private showCurrentEntry_: boolean;
  private currentPINError_: string;
  private newPINError_: string;
  private confirmPINError_: string;
  private complete_: boolean;
  private shown_: SetPinDialogPage;
  private pinsVisible_: boolean;
  private title_: string;
  private newPINDialogDescription_: string;
  private browserProxy_: SecurityKeysPinBrowserProxy =
      SecurityKeysPinBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.title_ = this.i18n('securityKeysSetPINInitialTitle');
    this.$.dialog.showModal();

    this.browserProxy_.startSetPin().then(
        ({done, error, currentMinPinLength, newMinPinLength, retries}) => {
          if (done) {
            // Operation is complete. error is a CTAP error code. See
            // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
            if (error === 1 /* INVALID_COMMAND */) {
              this.shown_ = SetPinDialogPage.NO_PIN_SUPPORT;
              this.finish_();
            } else if (error === 52 /* temporarily locked */) {
              this.shown_ = SetPinDialogPage.REINSERT;
              this.finish_();
            } else if (error === 50 /* locked */) {
              this.shown_ = SetPinDialogPage.LOCKED;
              this.finish_();
            } else {
              this.errorCode_ = error;
              this.shown_ = SetPinDialogPage.ERROR;
              this.finish_();
            }
          } else if (retries === 0) {
            // A device can also signal that it is locked by returning zero
            // retries.
            this.shown_ = SetPinDialogPage.LOCKED;
            this.finish_();
          } else {
            // Need to prompt for a pin. Initially set the text boxes to valid
            // so that they don't all appear red without the user typing
            // anything.
            this.currentPINValid_ = true;
            this.newPINValid_ = true;
            this.confirmPINValid_ = true;
            this.setPINButtonValid_ = true;

            this.currentMinPinLength_ = currentMinPinLength;
            this.newMinPinLength_ = newMinPinLength;
            this.retries_ = retries;
            // retries_ may be null to indicate that there is currently no PIN
            // set.
            let focusTarget: HTMLElement;
            if (this.retries_ === null) {
              this.showCurrentEntry_ = false;
              focusTarget = this.$.newPIN;
              this.title_ = this.i18n('securityKeysSetPINCreateTitle');
            } else {
              this.showCurrentEntry_ = true;
              focusTarget = this.$.currentPIN;
              this.title_ = this.i18n('securityKeysSetPINChangeTitle');
            }

            this.shown_ = SetPinDialogPage.PIN_PROMPT;
            // Focus cannot be set directly from within a backend callback.
            window.setTimeout(function() {
              focusTarget.focus();
            }, 0);
            this.fire_('ui-ready');  // for test synchronization.
          }
        });
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private closeDialog_() {
    this.$.dialog.close();
    this.finish_();
  }

  private finish_() {
    if (this.complete_) {
      return;
    }
    this.complete_ = true;
    // Setting |complete_| to true hides the |pinSubmitNew| button while it
    // has focus, which in turn causes the browser to move focus to the <body>
    // element, which in turn prevents subsequent "Enter" keystrokes to be
    // handled by cr-dialog itself. Re-focusing manually fixes this.
    this.$.dialog.focus();
    this.browserProxy_.close();
  }

  private onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  }

  private onCurrentPinInput_() {
    // Typing in the current PIN box after an error makes the error message
    // disappear.
    this.currentPINError_ = '';
  }

  private onNewPinInput_() {
    // Typing in the new PIN box after an error makes the error message
    // disappear.
    this.newPINError_ = '';
  }

  private onConfirmPinInput_() {
    // Typing in the confirm PIN box after an error makes the error message
    // disappear.
    this.confirmPINError_ = '';
  }

  /**
    @param pin A candidate PIN.
    @return An error string or else '' to indicate validity.
  */
  private isValidPin_(pin: string, minLength: number): string {
    // The UTF-8 encoding of the PIN must be between minLength and 63 bytes, and
    // the final byte cannot be zero.
    const utf8Encoded = new TextEncoder().encode(pin);
    if (utf8Encoded.length < minLength) {
      return this.i18n('securityKeysPINTooShort');
    }
    if (utf8Encoded.length > 63 ||
        // If the PIN somehow has a NUL at the end then it's invalid, but this
        // is so obscure that we don't try to message it. Rather we just say
        // that it's too long because trimming the final character is the best
        // response by the user.
        utf8Encoded[utf8Encoded.length - 1] === 0) {
      return this.i18n('securityKeysPINTooLong');
    }

    // A PIN must contain at least four code-points. Javascript strings are
    // UCS-2 and the |length| property counts UCS-2 elements, not code-points.
    // (For example, '\u{1f6b4}'.length === 2, but it's a single code-point.)
    // Therefore, iterate over the string (which does yield codepoints) and
    // check that |minLength| or more were seen.
    let length = 0;
    for (const _codepoint of pin) {
      length++;
    }

    if (length < minLength) {
      return this.i18n('securityKeysPINTooShort');
    }

    return '';
  }

  /**
   * @param retries The number of PIN attempts remaining.
   * @return The message to show under the text box.
   */
  private mismatchError_(retries: number): string {
    // Warn the user if the number of retries is getting low.
    if (1 < retries && retries <= 3) {
      return this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    }
    if (retries === 1) {
      return this.i18n('securityKeysPINIncorrectRetriesSin');
    }
    return this.i18n('securityKeysPINIncorrect');
  }

  /**
   * Called to set focus from inside a callback.
   */
  private focusOn_(focusTarget: HTMLElement) {
    // Focus cannot be set directly from within a backend callback. Also,
    // directly focusing |currentPIN| doesn't always seem to work(!). Thus
    // focus something else first, which is a hack that seems to solve the
    // problem.
    let preFocusTarget = this.$.newPIN;
    if (preFocusTarget === focusTarget) {
      preFocusTarget = this.$.currentPIN;
    }
    window.setTimeout(function() {
      preFocusTarget.focus();
      focusTarget.focus();
    }, 0);
  }

  /**
   * Called by Polymer when the Set PIN button is activated.
   */
  private pinSubmitNew_() {
    if (this.showCurrentEntry_) {
      this.currentPINError_ =
          this.isValidPin_(this.currentPIN_, this.currentMinPinLength_!);
      if (this.currentPINError_ !== '') {
        this.focusOn_(this.$.currentPIN);
        this.fire_('ui-ready');  // for test synchronization.
        return;
      }
    }

    this.newPINError_ = this.isValidPin_(this.newPIN_, this.newMinPinLength_!);
    if (this.newPINError_ !== '') {
      this.focusOn_(this.$.newPIN);
      this.fire_('ui-ready');  // for test synchronization.
      return;
    }

    if (this.newPIN_ !== this.confirmPIN_) {
      this.confirmPINError_ = this.i18n('securityKeysPINMismatch');
      this.focusOn_(this.$.confirmPIN);
      this.fire_('ui-ready');  // for test synchronization.
      return;
    }

    if (this.newPIN_ === this.currentPIN_) {
      this.newPINError_ = this.i18n('securityKeysSamePINAsCurrent');
      this.focusOn_(this.$.newPIN);
      this.fire_('ui-ready');  // for test synchronization.
      return;
    }

    this.setPINButtonValid_ = false;
    this.browserProxy_.setPin(this.currentPIN_, this.newPIN_).then(response => {
      const error = response.error;
      // This call always completes the process so response.done is always
      // true. error is a CTAP2 error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (error === 0 /* SUCCESS */) {
        this.shown_ = SetPinDialogPage.SUCCESS;
        this.finish_();
      } else if (error === 52 /* temporarily locked */) {
        this.shown_ = SetPinDialogPage.REINSERT;
        this.finish_();
      } else if (error === 50 /* locked */) {
        this.shown_ = SetPinDialogPage.LOCKED;
        this.finish_();
      } else if (error === 49 /* PIN_INVALID */) {
        this.currentPINValid_ = false;
        this.retries_!--;
        this.currentPINError_ = this.mismatchError_(this.retries_!);
        this.setPINButtonValid_ = true;
        this.focusOn_(this.$.currentPIN);
        this.fire_('ui-ready');  // for test synchronization.
      } else {
        // Unknown error.
        this.errorCode_ = error;
        this.shown_ = SetPinDialogPage.ERROR;
        this.finish_();
      }
    });
  }

  /**
   * onClick handler for the show/hide icon.
   */
  private showPinsClick_() {
    this.pinsVisible_ = !this.pinsVisible_;
  }

  /**
   * Polymer helper function to detect when an error string is empty.
   */
  private isNonEmpty_(s: string): boolean {
    return s !== '';
  }

  /**
   * Called by Polymer when |errorCode_| changes to set the error string.
   */
  private pinFailed_() {
    if (this.errorCode_ === null) {
      return '';
    }
    return this.i18n('securityKeysPINError', this.errorCode_!.toString());
  }

  /**
   * @return The class of the Ok / Cancel button.
   */
  private maybeActionButton_(): string {
    return this.complete_ ? 'action-button' : 'cancel-button';
  }

  /**
   * @return The label of the Ok / Cancel button.
   */
  private closeText_(): string {
    return this.i18n(this.complete_ ? 'ok' : 'cancel');
  }

  private newMinPinLengthChanged_() {
    PluralStringProxyImpl.getInstance()
        .getPluralString('securityKeysNewPIN', this.newMinPinLength_!)
        .then(string => this.newPINDialogDescription_ = string);
  }

  /**
   * @return The class (and thus icon) to be displayed.
   */
  private showPinsClass_(): string {
    return 'icon-visibility' + (this.pinsVisible_ ? '-off' : '');
  }

  /**
   * @return The tooltip for the icon.
   */
  private showPinsTitle_(): string {
    return this.i18n(
        this.pinsVisible_ ? 'securityKeysHidePINs' : 'securityKeysShowPINs');
  }

  /**
   * @return The PIN-input element type.
   */
  private inputType_(): string {
    return this.pinsVisible_ ? 'text' : 'password';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-keys-set-pin-dialog':
        SettingsSecurityKeysSetPinDialogElement;
  }
}

customElements.define(
    SettingsSecurityKeysSetPinDialogElement.is,
    SettingsSecurityKeysSetPinDialogElement);
