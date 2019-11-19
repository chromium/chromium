// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-set-pin-dialog' is a dialog for
 * setting and changing security key PINs.
 */

cr.define('settings', function() {
  /** @enum {string} */
  const SetPINDialogPage = {
    INITIAL: 'initial',
    NO_PIN_SUPPORT: 'noPINSupport',
    REINSERT: 'reinsert',
    LOCKED: 'locked',
    ERROR: 'error',
    PIN_PROMPT: 'pinPrompt',
    SUCCESS: 'success',
  };

  return {
    SetPINDialogPage: SetPINDialogPage,
  };
});

(function() {
'use strict';

const SetPINDialogPage = settings.SetPINDialogPage;

Polymer({
  is: 'settings-security-keys-set-pin-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether the value of the current PIN textbox is a valid PIN or not.
     * @private
     */
    currentPINValid_: Boolean,

    /** @private */
    newPINValid_: Boolean,

    /** @private */
    confirmPINValid_: Boolean,

    /**
     * Whether the dialog is in a state where the Set PIN button should be
     * enabled. Read by Polymer.
     * @private
     */
    setPINButtonValid_: {
      type: Boolean,
      value: false,
    },

    /**
     * The value of the new PIN textbox. Read/write by Polymer.
     * @private
     */
    newPIN_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmPIN_: {
      type: String,
      value: '',
    },

    /** @private */
    currentPIN_: {
      type: String,
      value: '',
    },

    /**
     * The number of PIN attempts remaining.
     * @private
     */
    retries_: Number,

    /**
     * A CTAP error code when we don't recognise the specific error. Read by
     * Polymer.
     * @private
     */
    errorCode_: Number,

    /**
     * Whether an entry for the current PIN should be displayed. (If no PIN
     * has been set then it won't be shown.)
     * @private
     */
    showCurrentEntry_: {
      type: Boolean,
      value: false,
    },

    /**
     * Error string to display under the current PIN entry, or empty.
     * @private
     */
    currentPINError_: {
      type: String,
      value: '',
    },

    /**
     * Error string to display under the new PIN entry, or empty.
     * @private
     */
    newPINError_: {
      type: String,
      value: '',
    },

    /**
     * Error string to display under the confirmation PIN entry, or empty.
     * @private
     */
    confirmPINError_: {
      type: String,
      value: '',
    },

    /**
     * Whether the dialog process has completed, successfully or otherwise.
     * @private
     */
    complete_: {
      type: Boolean,
      value: false,
    },

    /**
     * The id of an element on the page that is currently shown.
     * @private {!settings.SetPINDialogPage}
     */
    shown_: {
      type: String,
      value: SetPINDialogPage.INITIAL,
    },

    /**
     * Whether the contents of the PIN entries are visible, or are displayed
     * like passwords.
     * @private
     */
    pinsVisible_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    title_: String,
  },

  /** @private {?settings.SecurityKeysPINBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.title_ = this.i18n('securityKeysSetPINInitialTitle');
    this.browserProxy_ = settings.SecurityKeysPINBrowserProxyImpl.getInstance();
    this.$.dialog.showModal();

    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });

    this.browserProxy_.startSetPIN().then(([success, errorCode]) => {
      if (success) {
        // Operation is complete. errorCode is a CTAP error code. See
        // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
        if (errorCode == 1 /* INVALID_COMMAND */) {
          this.shown_ = SetPINDialogPage.NO_PIN_SUPPORT;
          this.finish_();
        } else if (errorCode == 52 /* temporarily locked */) {
          this.shown_ = SetPINDialogPage.REINSERT;
          this.finish_();
        } else if (errorCode == 50 /* locked */) {
          this.shown_ = SetPINDialogPage.LOCKED;
          this.finish_();
        } else {
          this.errorCode_ = errorCode;
          this.shown_ = SetPINDialogPage.ERROR;
          this.finish_();
        }
      } else if (errorCode == 0) {
        // A device can also signal that it is locked by returning zero retries.
        this.shown_ = SetPINDialogPage.LOCKED;
        this.finish_();
      } else {
        // Need to prompt for a pin. Initially set the text boxes to valid so
        // that they don't all appear red without the user typing anything.
        this.currentPINValid_ = true;
        this.newPINValid_ = true;
        this.confirmPINValid_ = true;
        this.setPINButtonValid_ = true;

        this.retries_ = errorCode;
        // retries_ may be null to indicate that there is currently no PIN set.
        let focusTarget;
        if (this.retries_ === null) {
          this.showCurrentEntry_ = false;
          focusTarget = this.$.newPIN;
          this.title_ = this.i18n('securityKeysSetPINCreateTitle');
        } else {
          this.showCurrentEntry_ = true;
          focusTarget = this.$.currentPIN;
          this.title_ = this.i18n('securityKeysSetPINChangeTitle');
        }

        this.shown_ = SetPINDialogPage.PIN_PROMPT;
        // Focus cannot be set directly from within a backend callback.
        window.setTimeout(function() {
          focusTarget.focus();
        }, 0);
        this.fire('ui-ready');  // for test synchronization.
      }
    });
  },

  /** @private */
  closeDialog_: function() {
    this.$.dialog.close();
    this.finish_();
  },

  /** @private */
  finish_: function() {
    if (this.complete_) {
      return;
    }
    this.complete_ = true;
    // Setting |complete_| to true hides the |pinSubmitNew| button while it
    // has focus, which in turn causes the browser to move focus to the <body>
    // element, which in turn prevents subsequent "Enter" keystrokes to be
    // handled by cr-dialog itself. Re-focusing manually fixes this.
    this.$.dialog.focus();
    this.browserProxy_.close();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIronSelect_: function(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering the
    // listener within settings-animated-pages.
    e.stopPropagation();
  },

  /** @private */
  onCurrentPINInput_: function() {
    // Typing in the current PIN box after an error makes the error message
    // disappear.
    this.currentPINError_ = '';
  },

  /** @private */
  onNewPINInput_: function() {
    // Typing in the new PIN box after an error makes the error message
    // disappear.
    this.newPINError_ = '';
  },

  /** @private */
  onConfirmPINInput_: function() {
    // Typing in the confirm PIN box after an error makes the error message
    // disappear.
    this.confirmPINError_ = '';
  },

  /**
    @param {string} pin A candidate PIN.
    @return {string} An error string or else '' to indicate validity.
    @private
  */
  isValidPIN_: function(pin) {
    // The UTF-8 encoding of the PIN must be between 4 and 63 bytes, and the
    // final byte cannot be zero.
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

  /**
   * @param {number} retries The number of PIN attempts remaining.
   * @return {string} The message to show under the text box.
   * @private
   */
  mismatchError_: function(retries) {
    // Warn the user if the number of retries is getting low.
    if (1 < retries && retries <= 3) {
      return this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    }
    if (retries == 1) {
      return this.i18n('securityKeysPINIncorrectRetriesSin');
    }
    return this.i18n('securityKeysPINIncorrect');
  },

  /**
   * Called to set focus from inside a callback.
   * @private
   */
  focusOn_: function(focusTarget) {
    // Focus cannot be set directly from within a backend callback. Also,
    // directly focusing |currentPIN| doesn't always seem to work(!). Thus
    // focus something else first, which is a hack that seems to solve the
    // problem.
    let preFocusTarget = this.$.newPIN;
    if (preFocusTarget == focusTarget) {
      preFocusTarget = this.$.currentPIN;
    }
    window.setTimeout(function() {
      preFocusTarget.focus();
      focusTarget.focus();
    }, 0);
  },

  /**
   * Called by Polymer when the Set PIN button is activated.
   * @private
   */
  pinSubmitNew_: function() {
    if (this.showCurrentEntry_) {
      this.currentPINError_ = this.isValidPIN_(this.currentPIN_);
      if (this.currentPINError_ != '') {
        this.focusOn_(this.$.currentPIN);
        this.fire('iron-announce', {text: this.currentPINError_});
        this.fire('ui-ready');  // for test synchronization.
        return;
      }
    }

    this.newPINError_ = this.isValidPIN_(this.newPIN_);
    if (this.newPINError_ != '') {
      this.focusOn_(this.$.newPIN);
      this.fire('iron-announce', {text: this.newPINError_});
      this.fire('ui-ready');  // for test synchronization.
      return;
    }

    if (this.newPIN_ != this.confirmPIN_) {
      this.confirmPINError_ = this.i18n('securityKeysPINMismatch');
      this.focusOn_(this.$.confirmPIN);
      this.fire('iron-announce', {text: this.confirmPINError_});
      this.fire('ui-ready');  // for test synchronization.
      return;
    }

    this.setPINButtonValid_ = false;
    this.browserProxy_.setPIN(this.currentPIN_, this.newPIN_).then(result => {
      // This call always completes the process so result[0] is always 1.
      // result[1] is a CTAP2 error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (result[1] == 0 /* SUCCESS */) {
        this.shown_ = SetPINDialogPage.SUCCESS;
        this.finish_();
      } else if (result[1] == 52 /* temporarily locked */) {
        this.shown_ = SetPINDialogPage.REINSERT;
        this.finish_();
      } else if (result[1] == 50 /* locked */) {
        this.shown_ = SetPINDialogPage.LOCKED;
        this.finish_();
      } else if (result[1] == 49 /* PIN_INVALID */) {
        this.currentPINValid_ = false;
        this.retries_--;
        this.currentPINError_ = this.mismatchError_(this.retries_);
        this.setPINButtonValid_ = true;
        this.focusOn_(this.$.currentPIN);
        this.fire('iron-announce', {text: this.currentPINError_});
        this.fire('ui-ready');  // for test synchronization.
      } else {
        // Unknown error.
        this.errorCode_ = result[1];
        this.shown_ = SetPINDialogPage.ERROR;
        this.finish_();
      }
    });
  },

  /**
   * onClick handler for the show/hide icon.
   * @private
   */
  showPINsClick_: function() {
    this.pinsVisible_ = !this.pinsVisible_;
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
   * Called by Polymer when |errorCode_| changes to set the error string.
   * @private
   */
  pinFailed_: function() {
    if (this.errorCode_ === null) {
      return '';
    }
    return this.i18n('securityKeysPINError', this.errorCode_.toString());
  },

  /**
   * @return {string} The class of the Ok / Cancel button.
   * @private
   */
  maybeActionButton_: function() {
    return this.complete_ ? 'action-button' : 'cancel-button';
  },

  /**
   * @return {string} The label of the Ok / Cancel button.
   * @private
   */
  closeText_: function() {
    return this.i18n(this.complete_ ? 'ok' : 'cancel');
  },

  /**
   * @return {string} The class (and thus icon) to be displayed.
   * @private
   */
  showPINsClass_: function() {
    return 'icon-visibility' + (this.pinsVisible_ ? '-off' : '');
  },

  /**
   * @return {string} The tooltip for the icon.
   * @private
   */
  showPINsTitle_: function() {
    return this.i18n(
        this.pinsVisible_ ? 'securityKeysHidePINs' : 'securityKeysShowPINs');
  },

  /**
   * @return {string} The PIN-input element type.
   * @private
   */
  inputType_: function() {
    return this.pinsVisible_ ? 'text' : 'password';
  },
});
})();
