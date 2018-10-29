// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-smb-share-dialog' is a component for adding
 * an SMB Share.
 */

Polymer({
  is: 'settings-add-smb-share-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private {string} */
    mountUrl_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    mountName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    username_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    password_: {
      type: String,
      value: '',
    },
    /** @private {!Array<string>}*/
    discoveredShares_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private */
    isActiveDirectory_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser');
      },
    },

    /** @private */
    authenticationMethod_: {
      type: String,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser') ?
            SmbAuthMethod.KERBEROS :
            SmbAuthMethod.CREDENTIALS;
      },
    },
  },

  /** @private {?settings.SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.startDiscovery();
    this.$.dialog.showModal();

    this.addWebUIListener('on-shares-found', this.onSharesFound_.bind(this));
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAddButtonTap_: function() {
    this.browserProxy_.smbMount(
        this.mountUrl_, this.mountName_.trim(), this.username_, this.password_,
        this.authenticationMethod_);
    this.$.dialog.close();
  },

  /** @private */
  onURLChanged_: function() {
    const parts = this.mountUrl_.split('\\');
    this.mountName_ = parts[parts.length - 1];
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddShare_: function() {
    return !!this.mountUrl_;
  },

  /**
   * @param {!Array<string>} shares
   * @private
   */
  onSharesFound_: function(shares) {
    this.discoveredShares_ = this.discoveredShares_.concat(shares);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialUI_: function() {
    return this.authenticationMethod_ == SmbAuthMethod.CREDENTIALS;
  },
});
