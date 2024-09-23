// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-iban-edit-dialog' is the dialog that allows
 * editing or creating an IBAN entry.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../i18n_setup.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iban_edit_dialog.html.js';
import type {PaymentsManagerProxy} from './payments_manager_proxy.js';
import {PaymentsManagerImpl} from './payments_manager_proxy.js';

/**
 * Enum of possible states for the iban. An iban is valid if it has the correct
 * structure, matches the length for its country code, and passes a checksum.
 * Otherwise, it is invalid and we may show an error to the user in cases where
 * we are certain they have entered an invalid iban (i.e. vs still typing).
 */
enum IbanValidationState {
  VALID = 'valid',
  INVALID_NO_ERROR = 'invalid-no-error',
  INVALID_WITH_ERROR = 'invalid-with-error',
}

declare global {
  interface HTMLElementEventMap {
    'save-iban': CustomEvent<chrome.autofillPrivate.IbanEntry>;
  }
}

export interface SettingsIbanEditDialogElement {
  $: {
    dialog: CrDialogElement,
    valueInput: CrInputElement,
    nicknameInput: CrInputElement,
    cancelButton: CrButtonElement,
    saveButton: CrButtonElement,
  };
}

const SettingsIbanEditDialogElementBase = I18nMixin(PolymerElement);

export class SettingsIbanEditDialogElement extends
    SettingsIbanEditDialogElementBase {
  static get is() {
    return 'settings-iban-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The IBAN being added or edited. Null means add a new IBAN, otherwise,
       * edit the existing IBAN.
       */
      iban: {
        type: Object,
        value: null,
      },

      /**
       * The actual title that's used for this dialog. Will be context sensitive
       * based on which type of IBAN method is being viewed, and if it is being
       * created or edited.
       */
      title_: String,

      /**
       * Backing data for inputs in the dialog, each bound to the corresponding
       * HTML elements.
       *
       * Note that value_ is unsanitized; code should instead use
       * `sanitizedIban_`.
       */
      value_: String,
      nickname_: String,

      /**
       * A sanitized version of `value_` with whitespace trimmed.
       */
      sanitizedIban_: {
        type: String,
        computed: 'sanitizeIban_(value_)',
        observer: 'onSanitizedIbanChanged_',
      },

      /** Whether the current iban field is invalid. */
      ibanValidationState_: {
        type: IbanValidationState,
        value: false,
      },
    };
  }

  iban: chrome.autofillPrivate.IbanEntry|null;
  private title_: string;
  private value_?: string;
  private nickname_?: string;
  private sanitizedIban_: string;
  private ibanValidationState_: IbanValidationState;
  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.iban) {
      // Save IBAN button is by default enabled in 'EDIT' mode as IBAN value is
      // pre-populated.
      this.value_ = this.iban.value;
      this.nickname_ = this.iban.nickname;
      this.title_ = this.i18n('editIbanTitle');
    } else {
      this.title_ = this.i18n('addIbanTitle');
      // Save IBAN button is disabled in 'ADD' mode as IBAN value is empty.
      this.$.saveButton.disabled = true;
    }
    this.$.dialog.showModal();
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for clicking the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * Handler for clicking the save button.
   */
  private onIbanSaveButtonClick_() {
    const iban = {
      guid: this.iban?.guid,
      value: this.sanitizedIban_,
      nickname: this.nickname_ ? this.nickname_.trim() : '',
    };
    this.dispatchEvent(new CustomEvent(
        'save-iban', {bubbles: true, composed: true, detail: iban}));
    this.close();
  }

  private async onSanitizedIbanChanged_() {
    this.ibanValidationState_ =
        await this.computeIbanValidationState_(/*isBlur=*/ false);
    this.$.saveButton.disabled =
        this.ibanValidationState_ !== IbanValidationState.VALID;
  }

  private async onIbanInputBlurred_(event: Event) {
    assert(event.type === 'blur');
    this.ibanValidationState_ =
        await this.computeIbanValidationState_(/*isBlur=*/ true);
  }

  private showErrorForIban_(ibanValidationState: IbanValidationState) {
    return ibanValidationState === IbanValidationState.INVALID_WITH_ERROR;
  }

  private sanitizeIban_(value: string): string {
    return value ? value.replace(/\s/g, '') : '';
  }

  private async computeIbanValidationState_(isBlur: boolean) {
    const isValid = await this.isValidIban();

    if (isValid) {
      return IbanValidationState.VALID;
    }

    // We do not want to show an 'invalid iban' error to users if they have not
    // yet finished typing the iban, but unfortunately different countries can
    // have different iban lengths so we do not know when the user might be
    // finished.
    //
    // In order to minimize false-positive errors, we only show an error if the
    // user has entered at least 24 characters (the average IBAN length), or if
    // they unfocus the IBAN input (which implies they are finished entering
    // their IBAN).
    return (this.sanitizedIban_.length >= 24 || isBlur) ?
        IbanValidationState.INVALID_WITH_ERROR :
        IbanValidationState.INVALID_NO_ERROR;
  }

  private async isValidIban(): Promise<boolean> {
    if (!this.sanitizedIban_) {
      return false;
    }
    return this.paymentsManager_.isValidIban(this.sanitizedIban_);
  }

  /**
   * @param  nickname of the IBAN, undefined when not set.
   * @return nickname character length.
   */
  private computeNicknameCharCount_(): number {
    return (this.nickname_ || '').length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-iban-edit-dialog': SettingsIbanEditDialogElement;
  }
}

customElements.define(
    SettingsIbanEditDialogElement.is, SettingsIbanEditDialogElement);
