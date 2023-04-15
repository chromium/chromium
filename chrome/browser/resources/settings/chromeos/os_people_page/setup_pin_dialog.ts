// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-setup-pin-dialog' is the settings page for choosing a PIN.
 *
 * Example:
 * * <settings-setup-pin-dialog set-modes="[[quickUnlockSetModes]]">
 * </settings-setup-pin-dialog>
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './setup_pin_dialog.html.js';

const SettingsSetupPinDialogElementBase = I18nMixin(PolymerElement);

interface SettingsSetupPinDialogElement {
  $: {
    dialog: CrDialogElement,
    pinKeyboard: PinKeyboardElement,
  };
}

class SettingsSetupPinDialogElement extends SettingsSetupPinDialogElementBase {
  static get is() {
    return 'settings-setup-pin-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Reflects property set in password_prompt_dialog.js.
       */
      setModes: {
        type: Object,
        notify: true,
      },

      /**
       * Should the step-specific submit button be displayed?
       */
      enableSubmit_: Boolean,

      /**
       * The current step/subpage we are on.
       */
      isConfirmStep_: {type: Boolean, value: false},

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       */
      quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},

      /**
       * writeUma is a function that handles writing uma stats. It may be
       * overridden for tests.
       */
      writeUma_: {
        type: Object,
        value() {
          return () => {};
        },
      },
    };
  }

  setModes: Object|null;
  private enableSubmit_: boolean;
  private isConfirmStep_: boolean;
  private quickUnlockPrivate: Object;
  private writeUma_: Function;

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.$.pinKeyboard.focus();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    this.$.pinKeyboard.resetState();
  }

  private onCancelClick_(): void {
    this.$.pinKeyboard.resetState();
    this.$.dialog.close();
  }

  private onPinSubmit_(): void {
    this.$.pinKeyboard.doSubmit();
  }

  private onSetPinDone_(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  private getTitleMessage_(isConfirmStep: boolean): string {
    return this.i18n(
        isConfirmStep ? 'configurePinConfirmPinTitle' :
                        'configurePinChoosePinTitle');
  }

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  private getContinueMessage_(isConfirmStep: boolean): string {
    return this.i18n(isConfirmStep ? 'confirm' : 'continue');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSetupPinDialogElement.is]: SettingsSetupPinDialogElement;
  }
}

customElements.define(
    SettingsSetupPinDialogElement.is, SettingsSetupPinDialogElement);
