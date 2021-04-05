// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_icon.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_shared_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'internet-config-dialog' is used to configure a new or existing network
 * outside of settings (e.g. from the login screen or when configuring a
 * new network from the system tray).
 */
Polymer({
  is: 'internet-config-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @private */
    shareAllowEnable_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shareNetworkAllowEnable');
      }
    },

    /** @private */
    shareDefault_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shareNetworkDefault');
      }
    },

    /**
     * The network GUID to configure, or empty when configuring a new network.
     * @private
     */
    guid_: String,

    /**
     * The type of network to be configured as a string. May be set initially or
     * updated by network-config.
     * @private
     */
    type_: String,

    /** @private */
    enableConnect_: Boolean,

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
  attached() {
    var dialogArgs = chrome.getVariableValue('dialogArguments');
    if (dialogArgs) {
      var args = JSON.parse(dialogArgs);
      this.type_ = args.type;
      assert(this.type_);
      this.guid_ = args.guid || '';
    } else {
      // For debugging
      var params = new URLSearchParams(document.location.search.substring(1));
      this.type_ = params.get('type') || 'WiFi';
      this.guid_ = params.get('guid') || '';
    }

    this.$.networkConfig.init();

    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /** @private */
  close_() {
    chrome.send('dialogClose');
  },

  /**
   * @return {string}
   * @private
   */
  getDialogTitle_() {
    var type = this.i18n('OncType' + this.type_);
    return this.i18n('internetJoinType', type);
  },

  /**
   * @return {string}
   * @private
   */
  getError_() {
    if (this.i18nExists(this.error_))
      return this.i18n(this.error_);
    return this.i18n('networkErrorUnknown');
  },

  /** @private */
  onCancelClick_() {
    this.close_();
  },

  /** @private */
  onConnectClick_() {
    this.$.networkConfig.connect();
  },
});
