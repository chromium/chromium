// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-reset-dialog' is a dialog for
 * triggering factory resets of security keys.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SecurityKeysResetBrowserProxy, SecurityKeysResetBrowserProxyImpl} from './security_keys_browser_proxy.js';

/** @enum {string} */
export const ResetDialogPage = {
  INITIAL: 'initial',
  NO_RESET: 'noReset',
  RESET_FAILED: 'resetFailed',
  RESET_CONFIRM: 'resetConfirm',
  RESET_SUCCESS: 'resetSuccess',
  RESET_NOT_ALLOWED: 'resetNotAllowed',
};


Polymer({
  is: 'settings-security-keys-reset-dialog',

  _template: html`{__html_template__}`,

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
     * @private {!ResetDialogPage}
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

  /** @private {?SecurityKeysResetBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.title_ = this.i18n('securityKeysResetTitle');
    this.browserProxy_ = SecurityKeysResetBrowserProxyImpl.getInstance();
    this.$.dialog.showModal();

    this.browserProxy_.reset().then(code => {
      // code is a CTAP error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (code === 1 /* INVALID_COMMAND */) {
        this.shown_ = ResetDialogPage.NO_RESET;
        this.finish_();
      } else if (code !== 0 /* unknown error */) {
        this.errorCode_ = code;
        this.shown_ = ResetDialogPage.RESET_FAILED;
        this.finish_();
      } else {
        this.title_ = this.i18n('securityKeysResetConfirmTitle');
        this.shown_ = ResetDialogPage.RESET_CONFIRM;
        this.browserProxy_.completeReset().then(code => {
          this.title_ = this.i18n('securityKeysResetTitle');
          if (code === 0 /* SUCCESS */) {
            this.shown_ = ResetDialogPage.RESET_SUCCESS;
          } else if (code === 48 /* NOT_ALLOWED */) {
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
  closeDialog_() {
    this.$.dialog.close();
    this.finish_();
  },

  /** @private */
  finish_() {
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
  onIronSelect_(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  },

  /**
     @param {number} code CTAP error code.
     @return {string} Contents of the error string that may be displayed
          to the user. Used automatically by Polymer.
     @private
   */
  resetFailed_(code) {
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
  closeText_(complete) {
    return this.i18n(complete ? 'ok' : 'cancel');
  },

  /**
   * @param {boolean} complete Whether the dialog process is complete.
   * @return {string} The class of the dialog button. Used automatically by
   *     Polymer.
   * @private
   */
  maybeActionButton_(complete) {
    return complete ? 'action-button' : 'cancel-button';
  },
});
