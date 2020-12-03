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
    /** @type {string} */
    esimProfileName_: {
      type: String,
      value: '',
    },

    /** @type {string} */
    iccid: {
      type: String,
      value: '',
    }
  },

  /**
   * Provides an interface to the ESimManager Mojo service.
   * @private {?chromeos.cellularSetup.mojom.ESimManagerRemote}
   */
  eSimManagerRemote_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @private {?chromeos.cellularSetup.mojom.ESimProfileRemote} */
  esimProfileRemote_: null,

  /** @override */
  created() {
    this.eSimManagerRemote_ = cellular_setup.getESimManagerRemote();
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    this.init_();
  },

  /** @private */
  async init_() {
    const response = await this.eSimManagerRemote_.getAvailableEuiccs();
    const euicc = response.euiccs[0];

    const esimProfilesRemotes = await euicc.getProfileList();

    for (const profileRemote of esimProfilesRemotes.profiles) {
      const profileProperties = await profileRemote.getProperties();

      if (profileProperties.properties.iccid !== this.iccid) {
        continue;
      }

      this.esimProfileRemote_ = profileRemote;
      this.esimProfileName_ = profileProperties.properties.nickname ?
          this.convertString16ToJSString_(
              profileProperties.properties.nickname) :
          this.convertString16ToJSString_(profileProperties.properties.name);
    }
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
  async onRemoveProfileTap_(event) {
    const response = await this.esimProfileRemote_.uninstallProfile();
    if (response.result ===
        chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
      console.error('Unable to remove profile');
      // TODO(crbug.com/1093185): Show useful error to user when uninstall fails
    }

    this.$.dialog.close();
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.dialog.close();
  }
});
