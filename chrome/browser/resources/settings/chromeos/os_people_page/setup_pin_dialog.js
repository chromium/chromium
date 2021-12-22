// Copyright 2016 The Chromium Authors. All rights reserved.
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

import '//resources/cr_components/chromeos/quick_unlock/setup_pin_keyboard.m.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-setup-pin-dialog',

  behaviors: [I18nBehavior],

  properties: {
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
      }
    },
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
    this.$.pinKeyboard.focus();
  },

  close() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }

    this.$.pinKeyboard.resetState();
  },


  /** @private */
  onCancelTap_() {
    this.$.pinKeyboard.resetState();
    this.$.dialog.close();
  },

  /** @private */
  onPinSubmit_() {
    this.$.pinKeyboard.doSubmit();
  },


  /** @private */
  onSetPinDone_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getTitleMessage_(isConfirmStep) {
    return this.i18n(
        isConfirmStep ? 'configurePinConfirmPinTitle' :
                        'configurePinChoosePinTitle');
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getContinueMessage_(isConfirmStep) {
    return this.i18n(isConfirmStep ? 'confirm' : 'continue');
  },
});
