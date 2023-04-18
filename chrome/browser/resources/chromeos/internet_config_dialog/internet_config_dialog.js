// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_config.js';
import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
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
      },
    },

    /** @private */
    shareDefault_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shareNetworkDefault');
      },
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

    /**
     * Whether the Jelly feature flag is enabled.
     * @private
     */
    isJellyEnabled_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.valueExists('isJellyEnabled') &&
            loadTimeData.getBoolean('isJellyEnabled');
      },
    },
  },

  /** @override */
  attached() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (dialogArgs) {
      const args = JSON.parse(dialogArgs);
      this.type_ = args.type;
      assert(this.type_);
      this.guid_ = args.guid || '';
    } else {
      // For debugging
      const params = new URLSearchParams(document.location.search.substring(1));
      this.type_ = params.get('type') || 'WiFi';
      this.guid_ = params.get('guid') || '';
    }

    if (this.isJellyEnabled_) {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'chrome://theme/colors.css?sets=legacy,sys';
      document.head.appendChild(link);
      document.body.classList.add('jelly-enabled');
      startColorChangeUpdater();
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
    const type = this.i18n('OncType' + this.type_);
    return this.i18n('internetJoinType', type);
  },

  /**
   * @return {string}
   * @private
   */
  getError_() {
    if (this.i18nExists(this.error_)) {
      return this.i18n(this.error_);
    }
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
