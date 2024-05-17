// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-setup-pin-dialog' is the dialog for setting up a PIN
 * to access App Parental Controls.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_setup_pin_dialog.html.js';
import {AppSetupPinKeyboardElement} from './app_setup_pin_keyboard.js';

const AppSetupPinDialogElementBase = I18nMixin(PolymerElement);

interface AppSetupPinDialogElement {
  $: {
    dialog: CrDialogElement,
    setupPinKeyboard: AppSetupPinKeyboardElement,
  };
}

class AppSetupPinDialogElement extends AppSetupPinDialogElementBase {
  static get is() {
    return 'app-setup-pin-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the user is at the PIN confirmation step.
       */
      isConfirmStep_: {type: Boolean, value: false},
    };
  }

  private isConfirmStep_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.$.setupPinKeyboard.focus();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    this.$.setupPinKeyboard.resetState();
  }

  private onCancelClick_(e: Event): void {
    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
    this.$.setupPinKeyboard.resetState();
    this.close();
  }

  private onPinSubmit_(e: Event): void {
    // TODO(b/332936223): This method currently naively switches from the
    // initial screen to the submit screen. It will be updated when the
    // actual PIN setup flow is implemented.

    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
    if (!this.isConfirmStep_) {
      this.$.setupPinKeyboard.doSubmit();
      this.isConfirmStep_ = true;
    } else {
      this.close();
    }
  }

  private getTitle_(isConfirmStep: boolean): string {
    return this.i18n(
        isConfirmStep ? 'appParentalControlsConfirmPinTitle' :
                        'appParentalControlsChoosePinTitle');
  }

  private getSubtitle_(isConfirmStep: boolean): string {
    return isConfirmStep ? '' :
                           this.i18n('appParentalControlsChoosePinSubtitle');
  }

  private getContinueMessage_(isConfirmStep: boolean): string {
    return this.i18n(isConfirmStep ? 'confirm' : 'continue');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppSetupPinDialogElement.is]: AppSetupPinDialogElement;
  }
}

customElements.define(AppSetupPinDialogElement.is, AppSetupPinDialogElement);
