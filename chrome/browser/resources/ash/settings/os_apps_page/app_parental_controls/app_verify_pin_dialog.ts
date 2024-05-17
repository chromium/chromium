// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-verify-pin-dialog' is the dialog for verifying that the PIN
 * a user enters to access App Parental Controls matches the existing PIN.
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';
import './app_setup_pin_keyboard.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_verify_pin_dialog.html.js';

const AppVerifyPinDialogElementBase = I18nMixin(PolymerElement);

interface AppVerifyPinDialogElement {
  $: {
    dialog: CrDialogElement,
    pinKeyboard: PinKeyboardElement,
  };
}

class AppVerifyPinDialogElement extends AppVerifyPinDialogElementBase {
  static get is() {
    return 'app-verify-pin-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current PIN keyboard value.
       */
      pinValue_: {
        type: String,
      },
    };
  }

  private pinValue_: string;

  override connectedCallback(): void {
    super.connectedCallback();

    this.resetState();
    this.$.dialog.showModal();
    this.$.pinKeyboard.focusInput();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
    this.resetState();
  }

  resetState(): void {
    this.pinValue_ = '';
  }

  private onCancelClick_(e: Event): void {
    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
    this.close();
  }

  private onPinSubmit_(): void {
    // TODO(b/332936481): Implement pin submission logic.
    return;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppVerifyPinDialogElement.is]: AppVerifyPinDialogElement;
  }
}

customElements.define(AppVerifyPinDialogElement.is, AppVerifyPinDialogElement);
