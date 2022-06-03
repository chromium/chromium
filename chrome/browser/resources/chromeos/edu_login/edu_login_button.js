// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const ButtonTypes = {
  OK: 'ok',
  NEXT: 'next',
  BACK: 'back'
};

Polymer({
  is: 'edu-login-button',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Set button type.
     * @type {!ButtonTypes}
     */
    buttonType: {
      type: String,
      value: ButtonTypes.OK,
    },
    /**
     * 'disabled' button attribute.
     * @type {Boolean}
     */
    disabled: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready() {
    assert(this.buttonType === ButtonTypes.OK ||
        this.buttonType === ButtonTypes.NEXT ||
        this.buttonType === ButtonTypes.BACK);
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} class name
   * @private
   */
  getClass_(buttonType) {
    return buttonType === ButtonTypes.BACK ? '' : 'action-button';
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} whether the button should have an icon before text
   * @private
   */
  hasIconBeforeText_(buttonType) {
    return buttonType === ButtonTypes.BACK;
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} whether the button should have an icon after text
   * @private
   */
  hasIconAfterText_(buttonType) {
    return buttonType === ButtonTypes.NEXT;
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} icon
   * @private
   */
  getIcon_(buttonType) {
    if (buttonType === ButtonTypes.NEXT) {
      return isRTL() ? 'cr:chevron-left' : 'cr:chevron-right';
    }
    if (buttonType === ButtonTypes.BACK) {
      return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
    }
    return '';
  },

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} localized button text
   * @private
   */
  getDisplayName_(buttonType) {
    if (buttonType === ButtonTypes.NEXT) {
      return this.i18n('nextButton');
    }
    if (buttonType === ButtonTypes.BACK) {
      return this.i18n('backButton');
    }
    return this.i18n('okButton');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTap_(e) {
    if (this.disabled) {
      e.stopPropagation();
      return;
    }
    this.fire(this.buttonType === ButtonTypes.BACK ? 'go-back' : 'go-next');
  }

});
