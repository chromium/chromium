// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-reset-dialog' is a dialog for
 * triggering factory resets of security keys.
 */

cr.define('settings', function() {
  /** @enum {string} */
  const ResetDialogPage = {
    INITIAL: 'initial',
    NO_RESET: 'noReset',
    RESET_FAILED: 'resetFailed',
    RESET_CONFIRM: 'resetConfirm',
    RESET_SUCCESS: 'resetSuccess',
    RESET_NOT_ALLOWED: 'resetNotAllowed',
  };


  return {
    ResetDialogPage: ResetDialogPage,
  };
});

(function() {
'use strict';

const ResetDialogPage = settings.ResetDialogPage;

Polymer({
  is: 'settings-security-keys-reset-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * A CTAP error code for when the specific error was not recognised.
     * @private
     */
    errorCode_: Number,

    /**
     * True iff the process has completed, successfully or otherwise.
     * @private
     */
    complete_: {
      type: Boolean,
      value: false,
    },

    /**
     * The id of an element on the page that is currently shown.
     * @private {!settings.ResetDialogPage}
     */
    shown_: {
      type: String,
      value: ResetDialogPage.INITIAL,
    },

    /**
     * @private
     */
    title_: String,
  },

  /** @private {?settings.SecurityKeysResetBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.title_ = this.i18n('securityKeysResetTitle');
    this.browserProxy_ =
        settings.SecurityKeysResetBrowserProxyImpl.getInstance();
    this.$.dialog.showModal();

    this.browserProxy_.reset().then(code => {
      // code is a CTAP error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (code == 1 /* INVALID_COMMAND */) {
        this.shown_ = ResetDialogPage.NO_RESET;
        this.finish_();
      } else if (code != 0 /* unknown error */) {
        this.errorCode_ = code;
        this.shown_ = ResetDialogPage.RESET_FAILED;
        this.finish_();
      } else {
        this.title_ = this.i18n('securityKeysResetConfirmTitle');
        this.shown_ = ResetDialogPage.RESET_CONFIRM;
        this.browserProxy_.completeReset().then(code => {
          this.title_ = this.i18n('securityKeysResetTitle');
          if (code == 0 /* SUCCESS */) {
            this.shown_ = ResetDialogPage.RESET_SUCCESS;
          } else if (code == 48 /* NOT_ALLOWED */) {
            this.shown_ = ResetDialogPage.RESET_NOT_ALLOWED;
          } else /* unknown error */ {
            this.errorCode_ = code;
            this.shown_ = ResetDialogPage.RESET_FAILED;
          }
          this.finish_();
        });
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

  /**
     @param {number} code CTAP error code.
     @return {string} Contents of the error string that may be displayed
          to the user. Used automatically by Polymer.
     @private
   */
  resetFailed_: function(code) {
    if (code === null) {
      return '';
    }
    return this.i18n('securityKeysResetError', code.toString());
  },

  /**
   * @param {boolean} complete Whether the dialog process is complete.
   * @return {string} The label of the dialog button. Used automatically by
   *     Polymer.
   * @private
   */
  closeText_: function(complete) {
    return this.i18n(complete ? 'ok' : 'cancel');
  },

  /**
   * @param {boolean} complete Whether the dialog process is complete.
   * @return {string} The class of the dialog button. Used automatically by
   *     Polymer.
   * @private
   */
  maybeActionButton_: function(complete) {
    return complete ? 'action-button' : 'cancel-button';
  },
});
})();
