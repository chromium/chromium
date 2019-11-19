// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-pin-field' is a component for entering
 * a security key PIN.
 */
Polymer({
  is: 'settings-security-keys-pin-field',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    // The validation error label of the input field.
    error: {
      type: String,
      observer: 'errorChanged_',
    },

    // The value of the input field.
    value: String,

    /**
     * Whether the contents of the input field are visible.
     * @private
     */
    inputVisible_: Boolean,
  },

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });

    this.inputVisible_ = false;
  },

  /** Focuses the PIN input field. */
  focus: function() {
    this.$.pin.focus();
  },

  /**
   * Validates the PIN and sets the validation error if it is not valid.
   * @return {boolean} True iff the PIN is valid.
   */
  validate: function() {
    const error = this.isValidPIN_(this.value);
    if (error != '') {
      this.error = error;
      return false;
    }
    return true;
  },

  /**
   * Sets the validation error to indicate the PIN was incorrect.
   * @param {number} retries The number of retries remaining.
   */
  showIncorrectPINError: function(retries) {
    // Warn the user if the number of retries is getting low.
    let error;
    if (1 < retries && retries <= 3) {
      error =
          this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    } else if (retries == 1) {
      error = this.i18n('securityKeysPINIncorrectRetriesSin');
    } else {
      error = this.i18n('securityKeysPINIncorrect');
    }
    this.error = error;
  },

  /** @private */
  onPINInput_: function() {
    // Typing in the PIN box after an error makes the error message
    // disappear.
    this.error = '';
  },

  /**
   * Polymer helper function to detect when an error string is empty.
   * @param {string} s Arbitrary string
   * @return {boolean} True iff |s| is non-empty.
   * @private
   */
  isNonEmpty_: function(s) {
    return s != '';
  },

  /**
   * @return {string} The PIN-input element type.
   * @private
   */
  inputType_: function() {
    return this.inputVisible_ ? 'text' : 'password';
  },

  /**
   * @return {string} The class (and thus icon) to be displayed.
   * @private
   */
  showButtonClass_: function() {
    return 'icon-visibility' + (this.inputVisible_ ? '-off' : '');
  },

  /**
   * @return {string} The tooltip for the icon.
   * @private
   */
  showButtonTitle_: function() {
    return this.i18n(
        this.inputVisible_ ? 'securityKeysHidePINs' : 'securityKeysShowPINs');
  },

  /**
   * onClick handler for the show/hide icon.
   * @private
   */
  showButtonClick_: function() {
    this.inputVisible_ = !this.inputVisible_;
  },

  /**
   * @param {string} pin A candidate PIN.
   * @return {string} An error string or else '' to indicate validity.
   * @private
   */
  isValidPIN_: function(pin) {
    // The UTF-8 encoding of the PIN must be between 4
    // and 63 bytes, and the final byte cannot be zero.
    const utf8Encoded = new TextEncoder().encode(pin);
    if (utf8Encoded.length < 4) {
      return this.i18n('securityKeysPINTooShort');
    }
    if (utf8Encoded.length > 63 ||
        // If the PIN somehow has a NUL at the end then it's invalid, but this
        // is so obscure that we don't try to message it. Rather we just say
        // that it's too long because trimming the final character is the best
        // response by the user.
        utf8Encoded[utf8Encoded.length - 1] == 0) {
      return this.i18n('securityKeysPINTooLong');
    }

    // A PIN must contain at least four code-points. Javascript strings are
    // UCS-2 and the |length| property counts UCS-2 elements, not code-points.
    // (For example, '\u{1f6b4}'.length == 2, but it's a single code-point.)
    // Therefore, iterate over the string (which does yield codepoints) and
    // check that four or more were seen.
    let length = 0;
    for (const codepoint of pin) {
      length++;
    }

    if (length < 4) {
      return this.i18n('securityKeysPINTooShort');
    }

    return '';
  },

  /** @private */
  errorChanged_: function() {
    // Make screen readers announce changes to the PIN validation error
    // label.
    this.fire('iron-announce', {text: this.error});
  },
});
