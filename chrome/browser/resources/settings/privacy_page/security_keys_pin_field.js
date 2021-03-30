// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-pin-field' is a component for entering
 * a security key PIN.
 */

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../settings_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

/**
 * A function that submits a PIN to a security key. It returns a Promise which
 * resolves with null if the PIN was correct, or with the number of retries
 * remaining otherwise.
 * @typedef function(string): Promise<?number>
 */
let PINFieldSubmitFunc;


Polymer({
  is: 'settings-security-keys-pin-field',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    minPinLength: {
      value: 4,
      type: Number,
    },

    /* @private */
    error_: {
      type: String,
      observer: 'errorChanged_',
    },

    /* @private */
    value_: String,

    /* @private */
    inputVisible_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  attached() {
    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  },

  /** Focuses the PIN input field. */
  focus() {
    this.$.pin.focus();
  },

  /**
   * Validates the PIN and sets the validation error if it is not valid.
   * @return {boolean} True iff the PIN is valid.
   * @private
   */
  validate_() {
    const error = this.isValidPIN_(this.value_);
    if (error !== '') {
      this.error_ = error;
      return false;
    }
    return true;
  },

  /**
   * Attempts submission of the PIN by invoking |submitFunc|. Updates the UI
   * to show an error if the PIN was incorrect.
   * @param {!PINFieldSubmitFunc} submitFunc
   * @return {!Promise} resolves if the PIN was correct, else rejects
   */
  trySubmit(submitFunc) {
    if (!this.validate_()) {
      this.focus();
      return Promise.reject();
    }
    return submitFunc(this.value_).then(retries => {
      if (retries != null) {
        this.showIncorrectPINError_(retries);
        this.focus();
        return Promise.reject();
      }
    });
  },

  /**
   * Sets the validation error to indicate the PIN was incorrect.
   * @param {number} retries The number of retries remaining.
   * @private
   */
  showIncorrectPINError_(retries) {
    // Warn the user if the number of retries is getting low.
    let error;
    if (1 < retries && retries <= 3) {
      error =
          this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    } else if (retries === 1) {
      error = this.i18n('securityKeysPINIncorrectRetriesSin');
    } else {
      error = this.i18n('securityKeysPINIncorrect');
    }
    this.error_ = error;
  },

  /** @private */
  onPINInput_() {
    // Typing in the PIN box after an error makes the error message
    // disappear.
    this.error_ = '';
  },

  /**
   * Polymer helper function to detect when an error string is empty.
   * @param {string} s Arbitrary string
   * @return {boolean} True iff |s| is non-empty.
   * @private
   */
  isNonEmpty_(s) {
    return s !== '';
  },

  /**
   * @return {string} The PIN-input element type.
   * @private
   */
  inputType_() {
    return this.inputVisible_ ? 'text' : 'password';
  },

  /**
   * @return {string} The class (and thus icon) to be displayed.
   * @private
   */
  showButtonClass_() {
    return 'icon-visibility' + (this.inputVisible_ ? '-off' : '');
  },

  /**
   * @return {string} The tooltip for the icon.
   * @private
   */
  showButtonTitle_() {
    return this.i18n(
        this.inputVisible_ ? 'securityKeysHidePINs' : 'securityKeysShowPINs');
  },

  /**
   * onClick handler for the show/hide icon.
   * @private
   */
  showButtonClick_() {
    this.inputVisible_ = !this.inputVisible_;
  },

  /**
   * @param {string} pin A candidate PIN.
   * @return {string} An error string or else '' to indicate validity.
   * @private
   */
  isValidPIN_(pin) {
    // The UTF-8 encoding of the PIN must be between minPinLength
    // and 63 bytes, and the final byte cannot be zero.
    const utf8Encoded = new TextEncoder().encode(pin);
    if (utf8Encoded.length < this.minPinLength) {
      return this.i18n('securityKeysPINTooShort');
    }
    if (utf8Encoded.length > 63 ||
        // If the PIN somehow has a NUL at the end then it's invalid, but this
        // is so obscure that we don't try to message it. Rather we just say
        // that it's too long because trimming the final character is the best
        // response by the user.
        utf8Encoded[utf8Encoded.length - 1] === 0) {
      return this.i18n('securityKeysPINTooLong');
    }

    // A PIN must contain at least minPinLength code-points. Javascript strings
    // are UCS-2 and the |length| property counts UCS-2 elements, not
    // code-points. (For example, '\u{1f6b4}'.length === 2, but it's a single
    // code-point.) Therefore, iterate over the string (which does yield
    // codepoints) and check that four or more were seen.
    let length = 0;
    for (const codepoint of pin) {
      length++;
    }

    if (length < this.minPinLength) {
      return this.i18n('securityKeysPINTooShort');
    }

    return '';
  },

  /** @private */
  errorChanged_() {
    // Make screen readers announce changes to the PIN validation error
    // label.
    this.fire('iron-announce', {text: this.error_});
  },
});
