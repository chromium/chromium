// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/quick_unlock/pin_keyboard.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import './oobe_icons.html.js';
import './buttons/oobe_back_button.js';
import './buttons/oobe_next_button.js';
import './common_styles/oobe_common_styles.css.js';
import './common_styles/oobe_dialog_host_styles.css.js';
import './dialogs/oobe_adaptive_dialog.js';

import {PinKeyboardElement} from '//resources/ash/common/quick_unlock/pin_keyboard.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.js';

import {OobeDialogHostMixin} from './mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {OobeTypes} from './oobe_types.js';
import {getTemplate} from './security_token_pin.html.js';

const SecurityTokenPinBase = OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

/**
 * @polymer
 */
export class SecurityTokenPin extends SecurityTokenPinBase {
  static get is() {
    return 'security-token-pin' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Contains the OobeTypes.SecurityTokenPinDialogParameters object. It can
       * be null when our element isn't used.
       *
       * Changing this field resets the dialog state. (Please note that, due to
       * the Polymer's limitation, only assigning a new object is observed;
       * changing just a subproperty won't work.)
       */
      parameters: {
        type: Object,
        observer: 'onParametersChanged',
      },

      /**
       * Whether the current state is the wait for the processing completion
       * (i.e., the backend is verifying the entered PIN).
       */
      processingCompletion: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the input is currently non-empty.
       */
      hasValue: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user has made changes in the input field since the dialog
       * was initialized or reset.
       */
      userEdited: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user can change the value in the input field.
       */
      canEdit: {
        type: Boolean,
        computed:
            'computeCanEdit(parameters.enableUserInput, processingCompletion)',
      },

      /**
       * Whether the user can submit a login request.
       */
      canSubmit: {
        type: Boolean,
        computed: 'computeCanSubmit(parameters.enableUserInput, ' +
            'hasValue, processingCompletion)',
      },
    };
  }

  parameters: object;
  private processingCompletion: boolean;
  private hasValue: boolean;
  private userEdited: boolean;
  private canEdit: boolean;
  private canSubmit: boolean;

  override focus(): void {
    // Note: setting the focus synchronously, to avoid flakiness in tests due to
    // racing between the asynchronous caret positioning and the PIN characters
    // input.
    this.getPinKeyboard().focusInputSynchronously();
  }

  /**
   * Computes the value of the canEdit property.
   */
  private computeCanEdit(
      enableUserInput: boolean, processingCompletion: boolean): boolean {
    return enableUserInput && !processingCompletion;
  }

  /**
   * Computes the value of the canSubmit property.
   */
  private computeCanSubmit(
      enableUserInput: boolean, hasValue: boolean,
      processingCompletion: boolean): boolean {
    return enableUserInput && hasValue && !processingCompletion;
  }

  /**
   * Invoked when the "Back" button is clicked.
   */
  private onBackClicked(): void {
    this.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
  }

  private getPinKeyboard(): PinKeyboardElement {
    const pinKeyboard =
        this.shadowRoot?.querySelector<PinKeyboardElement>('#pinKeyboard');
    assert(pinKeyboard instanceof PinKeyboardElement);
    return pinKeyboard;
  }

  /**
   * Invoked when the "Next" button is clicked or Enter is pressed.
   */
  private onSubmit(): void {
    if (!this.canSubmit) {
      // Disallow submitting when it's not allowed or while proceeding the
      // previous submission.
      return;
    }
    this.processingCompletion = true;

    this.dispatchEvent(new CustomEvent(
        'completed',
        {bubbles: true, composed: true, detail: this.getPinKeyboard().value}));
  }

  /**
   * Observer that is called when the |parameters| property gets changed.
   */
  private onParametersChanged(): void {
    // Reset the dialog to the initial state.
    this.getPinKeyboard().value = '';
    this.processingCompletion = false;
    this.hasValue = false;
    this.userEdited = false;

    this.focus();
  }

  /**
   * Observer that is called when the user changes the PIN input field.
   */
  private onPinChange(e: CustomEvent<{pin: string}>): void {
    this.hasValue = e.detail.pin.length > 0;
    this.userEdited = true;
  }

  /**
   * Returns whether the error label should be shown.
   */
  private isErrorLabelVisible(
      parameters: OobeTypes.SecurityTokenPinDialogParameters,
      userEdited: boolean): boolean {
    return parameters && parameters.hasError && !userEdited;
  }

  /**
   * Returns whether the PIN attempts left count should be shown.
   */
  private isAttemptsLeftVisible(
      parameters: OobeTypes.SecurityTokenPinDialogParameters): boolean {
    return parameters && parameters.formattedAttemptsLeft !== '';
  }

  /**
   * Returns whether there is a visible label for the PIN input field
   */
  private isLabelVisible(
      parameters: OobeTypes.SecurityTokenPinDialogParameters,
      userEdited: boolean): boolean {
    return this.isErrorLabelVisible(parameters, userEdited) ||
        this.isAttemptsLeftVisible(parameters);
  }

  /**
   * Returns the label to be used for the PIN input field.
   */
  private getLabel(
      parameters: OobeTypes.SecurityTokenPinDialogParameters,
      userEdited: boolean): string {
    if (!this.isLabelVisible(parameters, userEdited)) {
      // Neither error nor the number of left attempts are to be displayed.
      return '';
    }
    if (!this.isErrorLabelVisible(parameters, userEdited) &&
        this.isAttemptsLeftVisible(parameters)) {
      // There's no error, but the number of left attempts has to be displayed.
      return parameters.formattedAttemptsLeft;
    }
    return parameters.formattedError;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SecurityTokenPin.is]: SecurityTokenPin;
  }
}

customElements.define(SecurityTokenPin.is, SecurityTokenPin);
