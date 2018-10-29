// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-config-dialog' is used to configure a new or existing network
 * outside of settings (e.g. from the login screen or when configuring a
 * new network from the system tray).
 */
Polymer({
  is: 'internet-config-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /** @private {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy_: Object,

    /** @private */
    shareAllowEnable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('shareNetworkAllowEnable');
      }
    },

    /** @private */
    shareDefault_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('shareNetworkDefault');
      }
    },

    /**
     * The network GUID to configure, or empty when configuring a new network.
     * @private
     */
    guid_: String,

    /** @private */
    enableConnect_: Boolean,

    /**
     * The current properties if an existing network is being configured, or
     * a minimal subset for a new network. Note: network-config may modify
     * this (specifically .name).
     * @type {!chrome.networkingPrivate.ManagedProperties}
     */
    managedProperties_: Object,

    /**
     * Set by network-config when a configuration error occurs.
     * @private
     */
    error_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached: function() {
    var dialogArgs = chrome.getVariableValue('dialogArguments');
    var type;
    if (dialogArgs) {
      var args = JSON.parse(dialogArgs);
      type = args.type;
      assert(type);
      this.guid_ = args.guid || '';
    } else {
      // For debugging
      var params = new URLSearchParams(document.location.search.substring(1));
      type = params.get('type') || 'WiFi';
      this.guid_ = params.get('guid') || '';
    }

    this.managedProperties_ = {
      GUID: this.guid_,
      Name: {Active: ''},
      Type: /** @type {chrome.networkingPrivate.NetworkType} */ (type),
    };

    this.$.networkConfig.init();

    this.networkingPrivate.getGlobalPolicy(policy => {
      this.globalPolicy_ = policy;
    });

    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /** @private */
  close_: function() {
    chrome.send('dialogClose');
  },

  /**
   * @return {string}
   * @private
   */
  getDialogTitle_: function() {
    var type = this.i18n('OncType' + this.managedProperties_.Type);
    return this.i18n('internetJoinType', type);
  },

  /**
   * @return {string}
   * @private
   */
  getError_: function() {
    if (this.i18nExists(this.error_))
      return this.i18n(this.error_);
    return this.i18n('networkErrorUnknown');
  },

  /** @private */
  onCancelTap_: function() {
    this.close_();
  },

  /** @private */
  onConnectTap_: function() {
    this.$.networkConfig.connect();
  },
});
