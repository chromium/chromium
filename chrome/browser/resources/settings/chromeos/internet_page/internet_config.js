// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-config' is a Settings dialog wrapper for network-config.
 */
import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './internet_shared_css.js';

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {HTMLEscape} from 'chrome://resources/js/util.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const InternetConfigElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class InternetConfigElement extends InternetConfigElementBase {
  static get is() {
    return 'internet-config';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * The GUID when an existing network is being configured. This will be
       * empty when configuring a new network.
       */
      guid: {
        type: String,
        value: '',
      },

      /**
       * The type of network to be configured as a string. May be set initially
       * or updated by network-config.
       */
      type: String,

      /**
       * The name of the network. May be set initially or updated by
       * network-config.
       */
      name: String,

      /**
       * Set to true to show the 'connect' button instead of 'save'.
       */
      showConnect: Boolean,

      /** @private */
      enableConnect_: Boolean,

      /** @private */
      enableSave_: Boolean,

      /**
       * Set by network-config when a configuration error occurs.
       * @private
       */
      error_: {
        type: String,
        value: '',
      },
    };
  }

  open() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }

    this.$.networkConfig.init();
  }

  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClose_(event) {
    this.close();
  }

  /**
   * @return {string}
   * @private
   */
  getDialogTitle_() {
    if (this.name && !this.showConnect) {
      return this.i18n('internetConfigName', HTMLEscape(this.name));
    }
    const type = this.i18n('OncType' + this.type);
    return this.i18n('internetJoinType', type);
  }

  /**
   * @return {string}
   * @private
   */
  getError_() {
    if (this.i18nExists(this.error_)) {
      return this.i18n(this.error_);
    }
    return this.i18n('networkErrorUnknown');
  }

  /** @private */
  onCancelTap_() {
    this.close();
  }

  /**
   * Note that onSaveTap_ will only be called if the user explicitly clicks
   * on the 'Save' button.
   * @private
   */
  onSaveTap_() {
    /** @type {!NetworkConfigElement} */ (this.$.networkConfig).save();
  }

  /**
   * Note that onConnectTap_ will only be called if the user explicitly clicks
   * on the 'Connect' button.
   * @private
   */
  onConnectTap_() {
    /** @type {!NetworkConfigElement} */ (this.$.networkConfig).connect();
  }

  /**
   * A connect or save may be initiated within the NetworkConfigElement instead
   * of onConnectTap_() or onSaveTap_() (e.g on an enter event).
   * @private
   */
  onPropertiesSet_() {
    if (this.type ===
        OncMojo.getNetworkTypeString(
            chromeos.networkConfig.mojom.NetworkType.kWiFi)) {
      recordSettingChange(
          chromeos.settings.mojom.Setting.kWifiAddNetwork,
          {stringValue: this.guid});
    } else {
      recordSettingChange();
    }
  }
}

customElements.define(InternetConfigElement.is, InternetConfigElement);
