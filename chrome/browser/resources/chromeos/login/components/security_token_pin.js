// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/quick_unlock/pin_keyboard.js';
import '//resources/cr_elements/icons.html.js';
import './oobe_icons.html.js';
import './buttons/oobe_back_button.js';
import './buttons/oobe_next_button.js';
import './common_styles/oobe_common_styles.css.js';
import './common_styles/oobe_dialog_host_styles.css.js';
import './dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostBehavior} from './behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';
import {OobeTypes} from './oobe_types.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const SecurityTokenPinBase =
    mixinBehaviors([OobeI18nBehavior, OobeDialogHostBehavior], PolymerElement);

/**
 * @polymer
 */
class SecurityTokenPin extends SecurityTokenPinBase {
  static get is() {
    return 'security-token-pin';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
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
        observer: 'onParametersChanged_',
      },

      /**
       * Whether the current state is the wait for the processing completion
       * (i.e., the backend is verifying the entered PIN).
       * @private
       */
      processingCompletion_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the input is currently non-empty.
       * @private
       */
      hasValue_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user has made changes in the input field since the dialog
       * was initialized or reset.
       * @private
       */
      userEdited_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user can change the value in the input field.
       * @private
       */
      canEdit_: {
        type: Boolean,
        computed:
            'computeCanEdit_(parameters.enableUserInput, processingCompletion_)',
      },

      /**
       * Whether the user can submit a login request.
       * @private
       */
      canSubmit_: {
        type: Boolean,
        computed: 'computeCanSubmit_(parameters.enableUserInput, ' +
            'hasValue_, processingCompletion_)',
      },
    };
  }

  focus() {
    // Note: setting the focus synchronously, to avoid flakiness in tests due to
    // racing between the asynchronous caret positioning and the PIN characters
    // input.
    this.$.pinKeyboard.focusInputSynchronously();
  }

  /**
   * Computes the value of the canEdit_ property.
   * @param {boolean} enableUserInput
   * @param {boolean} processingCompletion
   * @return {boolean}
   * @private
   */
  computeCanEdit_(enableUserInput, processingCompletion) {
    return enableUserInput && !processingCompletion;
  }

  /**
   * Computes the value of the canSubmit_ property.
   * @param {boolean} enableUserInput
   * @param {boolean} hasValue
   * @param {boolean} processingCompletion
   * @return {boolean}
   * @private
   */
  computeCanSubmit_(enableUserInput, hasValue, processingCompletion) {
    return enableUserInput && hasValue && !processingCompletion;
  }

  /**
   * Invoked when the "Back" button is clicked.
   * @private
   */
  onBackClicked_() {
    this.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
  }

  /**
   * Invoked when the "Next" button is clicked or Enter is pressed.
   * @private
   */
  onSubmit_() {
    if (!this.canSubmit_) {
      // Disallow submitting when it's not allowed or while proceeding the
      // previous submission.
      return;
    }
    this.processingCompletion_ = true;
    this.dispatchEvent(new CustomEvent(
        'completed',
        {bubbles: true, composed: true, detail: this.$.pinKeyboard.value}));
  }

  /**
   * Observer that is called when the |parameters| property gets changed.
   * @private
   */
  onParametersChanged_() {
    // Reset the dialog to the initial state.
    this.$.pinKeyboard.value = '';
    this.processingCompletion_ = false;
    this.hasValue_ = false;
    this.userEdited_ = false;

    this.focus();
  }

  /**
   * Observer that is called when the user changes the PIN input field.
   * @param {!CustomEvent<{pin: string}>} e
   * @private
   */
  onPinChange_(e) {
    this.hasValue_ = e.detail.pin.length > 0;
    this.userEdited_ = true;
  }

  /**
   * Returns whether the error label should be shown.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  isErrorLabelVisible_(parameters, userEdited) {
    return parameters && parameters.hasError && !userEdited;
  }

  /**
   * Returns whether the PIN attempts left count should be shown.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {boolean}
   * @private
   */
  isAttemptsLeftVisible_(parameters) {
    return parameters && parameters.formattedAttemptsLeft !== '';
  }

  /**
   * Returns whether there is a visible label for the PIN input field
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  isLabelVisible_(parameters, userEdited) {
    return this.isErrorLabelVisible_(parameters, userEdited) ||
        this.isAttemptsLeftVisible_(parameters);
  }

  /**
   * Returns the label to be used for the PIN input field.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {string}
   * @private
   */
  getLabel_(parameters, userEdited) {
    if (!this.isLabelVisible_(parameters, userEdited)) {
      // Neither error nor the number of left attempts are to be displayed.
      return '';
    }
    if (!this.isErrorLabelVisible_(parameters, userEdited) &&
        this.isAttemptsLeftVisible_(parameters)) {
      // There's no error, but the number of left attempts has to be displayed.
      return parameters.formattedAttemptsLeft;
    }
    return parameters.formattedError;
  }
}

customElements.define(SecurityTokenPin.is, SecurityTokenPin);
