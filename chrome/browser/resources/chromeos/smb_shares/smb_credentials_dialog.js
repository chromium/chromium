// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'smb-credentials-dialog' is used to update the credentials for a mounted
 * smb share.
 */
Polymer({
  is: 'smb-credentials-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {string} */
    sharePath_: String,

    /** @private {string} */
    username_: String,

    /** @private {string} */
    password_: String,

    /** @private {number} */
    mountId_: {
      type: Number,
      value: -1,
    },
  },

  /** @private {?smb_shares.SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = smb_shares.SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    assert(dialogArgs);
    var args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.path);
    assert(typeof args.mid === 'number');
    this.sharePath_ = args.path;
    this.mountId_ = args.mid;

    this.$.dialog.showModal();
  },

  /** @private */
  onCancelButtonClick_: function() {
    chrome.send('dialogClose');
  },

  /** @private */
  onSaveButtonClick_: function() {
    this.browserProxy_.updateCredentials(
        this.mountId_, this.username_, this.password_);
    chrome.send('dialogClose');
  },
});