// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-password-prompt-dialog' shows a dialog which asks for the user to
 * enter their password. It validates the password is correct. Once the user has
 * entered their account password, the page fires an 'authenticated' event and
 * updates the authToken binding.
 *
 * Example:
 *
 * <settings-password-prompt-dialog
 *     id="passwordPrompt"
 *     password-prompt-text="{{passwordPromptText}}"
 *     auth-token="{{authToken}}">
 * </settings-password-prompt-dialog>
 */

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_prompt_dialog.html.js';

interface SettingsPasswordPromptDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsPasswordPromptDialogElement extends PolymerElement {
  static get is() {
    return 'settings-password-prompt-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The subtext to be displayed above the password input field. Embedders
       * may choose to change this value for their specific use case.
       */
      passwordPromptText: {
        type: String,
        notify: true,
        value: '',
      },

      inputValue_: {
        type: String,
        value: '',
        observer: 'onInputValueChange_',
      },

      /**
       * Helper property which marks password as valid/invalid.
       */
      passwordInvalid_: {
        type: Boolean,
        value: false,
      },

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       */
      quickUnlockPrivate: {
        type: Object,
        value: chrome.quickUnlockPrivate,
      },

      waitingForPasswordCheck_: {
        type: Boolean,
        value: false,
      },
    };
  }

  passwordPromptText: string;
  quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  private inputValue_: string;
  private passwordInvalid_: boolean;
  private waitingForPasswordCheck_: boolean;

  get passwordInput(): CrInputElement {
    return this.shadowRoot!.querySelector('cr-input')!;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
    // This needs to occur at the next paint otherwise the password input will
    // not receive focus.
    window.setTimeout(() => {
      // TODO(crbug.com/876377): This is unusual; the 'autofocus' attribute on
      // the cr-input element should work. Investigate.
      this.passwordInput.focus();
    }, 1);
  }

  private onCancelClick_(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  /**
   * Run the account password check.
   */
  private submitPassword_(): void {
    this.waitingForPasswordCheck_ = true;

    const password = this.passwordInput.value;
    // The user might have started entering a password and then deleted it all.
    // Do not submit/show an error in this case.
    if (!password) {
      this.passwordInvalid_ = false;
      this.waitingForPasswordCheck_ = false;
      return;
    }

    this.quickUnlockPrivate.getAuthToken(password, (tokenInfo) => {
      this.waitingForPasswordCheck_ = false;
      if (chrome.runtime.lastError) {
        this.passwordInvalid_ = true;
        // Select the whole password if user entered an incorrect password.
        this.passwordInput.select();
        return;
      }

      this.dispatchEvent(new CustomEvent(
          'token-obtained',
          {bubbles: true, composed: true, detail: tokenInfo}));
      this.passwordInvalid_ = false;

      if (this.$.dialog.open) {
        this.$.dialog.close();
      }
    });
  }

  private onInputValueChange_(): void {
    this.passwordInvalid_ = false;
  }

  private isConfirmEnabled_(): boolean {
    return !this.waitingForPasswordCheck_ && !this.passwordInvalid_ &&
        !!this.inputValue_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasswordPromptDialogElement.is]: SettingsPasswordPromptDialogElement;
  }
}

customElements.define(
    SettingsPasswordPromptDialogElement.is,
    SettingsPasswordPromptDialogElement);
