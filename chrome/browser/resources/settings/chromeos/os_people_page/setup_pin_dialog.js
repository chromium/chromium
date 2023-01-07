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

import 'chrome://resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsSetupPinDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsSetupPinDialogElement extends SettingsSetupPinDialogElementBase {
  static get is() {
    return 'settings-setup-pin-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Reflects property set in password_prompt_dialog.js.
       * @type {?Object}
       */
      setModes: {
        type: Object,
        notify: true,
      },

      /**
       * Should the step-specific submit button be displayed?
       * @private
       */
      enableSubmit_: Boolean,

      /**
       * The current step/subpage we are on.
       * @private
       */
      isConfirmStep_: {type: Boolean, value: false},

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       * @private
       */
      quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},

      /**
       * writeUma is a function that handles writing uma stats. It may be
       * overridden for tests.
       *
       * @type {Function}
       * @private
       */
      writeUma_: {
        type: Object,
        value() {
          return () => {};
        },
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.$.pinKeyboard.focus();
  }

  close() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    this.$.pinKeyboard.resetState();
  }


  /** @private */
  onCancelTap_() {
    this.$.pinKeyboard.resetState();
    this.$.dialog.close();
  }

  /** @private */
  onPinSubmit_() {
    this.$.pinKeyboard.doSubmit();
  }

  /** @private */
  onSetPinDone_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getTitleMessage_(isConfirmStep) {
    return this.i18n(
        isConfirmStep ? 'configurePinConfirmPinTitle' :
                        'configurePinChoosePinTitle');
  }

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getContinueMessage_(isConfirmStep) {
    return this.i18n(isConfirmStep ? 'confirm' : 'continue');
  }
}

customElements.define(
    SettingsSetupPinDialogElement.is, SettingsSetupPinDialogElement);
