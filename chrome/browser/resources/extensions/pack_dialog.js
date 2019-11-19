// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './pack_dialog_alert.js';
import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @interface */
export class PackDialogDelegate {
  /**
   * Opens a file browser for the user to select the root directory.
   * @return {Promise<string>} A promise that is resolved with the path the
   *     user selected.
   */
  choosePackRootDirectory() {}

  /**
   * Opens a file browser for the user to select the private key file.
   * @return {Promise<string>} A promise that is resolved with the path the
   *     user selected.
   */
  choosePrivateKeyPath() {}

  /**
   * Packs the extension into a .crx.
   * @param {string} rootPath
   * @param {string} keyPath
   * @param {number=} flag
   * @param {function(chrome.developerPrivate.PackDirectoryResponse)=}
   *     callback
   */
  packExtension(rootPath, keyPath, flag, callback) {}
}

Polymer({
  is: 'extensions-pack-dialog',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {PackDialogDelegate} */
    delegate: Object,

    /** @private */
    packDirectory_: {
      type: String,
      value: '',  // Initialized to trigger binding when attached.
    },

    /** @private */
    keyFile_: String,

    /** @private {?chrome.developerPrivate.PackDirectoryResponse} */
    lastResponse_: Object,
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  onRootBrowse_: function() {
    this.delegate.choosePackRootDirectory().then(path => {
      if (path) {
        this.set('packDirectory_', path);
      }
    });
  },

  /** @private */
  onKeyBrowse_: function() {
    this.delegate.choosePrivateKeyPath().then(path => {
      if (path) {
        this.set('keyFile_', path);
      }
    });
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onConfirmTap_: function() {
    this.delegate.packExtension(
        this.packDirectory_, this.keyFile_, 0, this.onPackResponse_.bind(this));
  },

  /**
   * @param {chrome.developerPrivate.PackDirectoryResponse} response the
   *    response from request to pack an extension.
   * @private
   */
  onPackResponse_: function(response) {
    this.lastResponse_ = response;
  },

  /**
   * In the case that the alert dialog was a success message, the entire
   * pack-dialog should close. Otherwise, we detach the alert by setting
   * lastResponse_ null. Additionally, if the user selected "proceed anyway"
   * in the dialog, we pack the extension again with override flags.
   * @param {!Event} e
   * @private
   */
  onAlertClose_: function(e) {
    e.stopPropagation();

    if (this.lastResponse_.status ==
        chrome.developerPrivate.PackStatus.SUCCESS) {
      this.$.dialog.close();
      return;
    }

    // This is only possible for a warning dialog.
    if (this.$$('extensions-pack-dialog-alert').returnValue == 'success') {
      this.delegate.packExtension(
          this.lastResponse_.item_path, this.lastResponse_.pem_path,
          this.lastResponse_.override_flags, this.onPackResponse_.bind(this));
    }

    this.lastResponse_ = null;
  },
});
