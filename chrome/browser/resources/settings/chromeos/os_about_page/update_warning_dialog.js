// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-update-warning-dialog' is a component warning the
 * user about update over mobile data. By clicking 'Continue', the user
 * agrees to download update using mobile data.
 */
Polymer({
  is: 'settings-update-warning-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!AboutPageUpdateInfo|undefined} */
    updateInfo: {
      type: Object,
      observer: 'updateInfoChanged_',
    },
  },

  /** @private {?settings.AboutPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onContinueTap_() {
    if (!this.updateInfo || !this.updateInfo.version || !this.updateInfo.size){
      console.log('ERROR: requestUpdateOverCellular arguments are undefined');
      return;
    }
    this.browserProxy_.requestUpdateOverCellular(
        /** @type {!string} */ (this.updateInfo.version),
        /** @type {!string} */ (this.updateInfo.size));
    this.$.dialog.close();
  },

  /** @private */
  updateInfoChanged_() {
    this.$$('#update-warning-message').innerHTML = this.i18n(
        'aboutUpdateWarningMessage',
        // Convert bytes to megabytes
        Math.floor(Number(this.updateInfo.size) / (1024 * 1024)));
  },
});
