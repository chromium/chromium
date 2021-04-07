// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to remove eSIM profile
 */

Polymer({
  is: 'esim-remove-profile-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {?OncMojo.NetworkStateProperties} */
    networkState: {
      type: Object,
      value: null,
    },

    /** @type {boolean} */
    showCellularDisconnectWarning: {
      type: Boolean,
      value: false,
    },

    /** @type {string} */
    esimProfileName_: {
      type: String,
      value: '',
    },
  },

  /** @private {?chromeos.cellularSetup.mojom.ESimProfileRemote} */
  esimProfileRemote_: null,

  /** @override */
  attached() {
    this.init_();
  },

  /** @private */
  async init_() {
    if (!(this.networkState &&
          this.networkState.type ===
              chromeos.networkConfig.mojom.NetworkType.kCellular)) {
      return;
    }
    this.esimProfileRemote_ = await cellular_setup.getESimProfile(
        this.networkState.typeState.cellular.iccid);
    // Fail gracefully if init is incomplete, see crbug/1194729.
    if (!this.esimProfileRemote_) {
      this.fire('show-error-toast', this.i18n('eSimRemoveProfileDialogError'));
      this.$.dialog.close();
      return;
    }
    this.esimProfileName_ = this.networkState.name;
  },

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   * @private
   */
  convertString16ToJSString_(str) {
    return str.data.map(ch => String.fromCodePoint(ch)).join('');
  },

  /**
   * @returns {string}
   * @private
   */
  getTitleString_() {
    return this.i18n('esimRemoveProfileDialogTitle', this.esimProfileName_);
  },

  /**
   * @param {Event} event
   * @private
   */
  onRemoveProfileTap_(event) {
    this.esimProfileRemote_.uninstallProfile().then((response) => {
      if (response.result ===
          chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
        this.fire(
            'show-error-toast', this.i18n('eSimRemoveProfileDialogError'));
      }
    });
    this.$.dialog.close();
    const params = new URLSearchParams;
    params.append(
        'type',
        OncMojo.getNetworkTypeString(
            chromeos.networkConfig.mojom.NetworkType.kCellular));
    settings.Router.getInstance().setCurrentRoute(
        settings.routes.INTERNET_NETWORKS, params, /*isPopState=*/ true);
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.dialog.close();
  }
});
