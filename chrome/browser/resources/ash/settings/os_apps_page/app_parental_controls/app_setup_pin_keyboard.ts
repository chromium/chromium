// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-setup-pin-keyboard' is the keyboard/input field for
 * entering a PIN to access App Parental Controls. Used by the PIN setup
 * dialog.
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_setup_pin_keyboard.html.js';

const AppSetupPinKeyboardElementBase = I18nMixin(PolymerElement);

export interface AppSetupPinKeyboardElement {
  $: {
    pinKeyboard: PinKeyboardElement,
  };
}

export class AppSetupPinKeyboardElement extends AppSetupPinKeyboardElementBase {
  static get is() {
    return 'app-setup-pin-keyboard' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the user is at the PIN confirmation step.
       */
      isConfirmStep: {
        notify: true,
        type: Boolean,
        value: false,
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
    };
  }

  isConfirmStep: boolean;
  private pinKeyboardValue_: string;
  private initialPin_: string;

  override focus(): void {
    this.$.pinKeyboard.focusInput();
  }

  override connectedCallback(): void {
    this.resetState();
  }

  resetState(): void {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.isConfirmStep = false;
  }

  /**
   * Intended to be called by the containing dialog when the user attempts
   * to submit the PIN.
   */
  doSubmit(): void {
    // TODO(b/332936223): Implement actual PIN submission logic.
    if (!this.isConfirmStep) {
      this.initialPin_ = this.pinKeyboardValue_;
      this.pinKeyboardValue_ = '';
      this.$.pinKeyboard.focusInput();
      this.isConfirmStep = true;
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppSetupPinKeyboardElement.is]: AppSetupPinKeyboardElement;
  }
}

customElements.define(
    AppSetupPinKeyboardElement.is, AppSetupPinKeyboardElement);
