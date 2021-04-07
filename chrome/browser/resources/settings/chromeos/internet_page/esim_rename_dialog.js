// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to rename eSIM profile name
 */

Polymer({
  is: 'esim-rename-dialog',

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

    /** @private {string} */
    esimProfileName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    errorMessage_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isRenameInProgress_: {
      type: Boolean,
      value: false,
    }
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
      this.errorMessage_ = this.i18n('eSimRenameProfileDialogError');
    }
    this.esimProfileName_ = this.networkState.name;
  },

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   */
  convertString16ToJSString_(str) {
    return str.data.map(ch => String.fromCodePoint(ch)).join('');
  },

  /**
   * @param {Event} event
   * @private
   */
  async onRenameDialogDoneTap_(event) {
    if (this.errorMessage_) {
      this.$.profileRenameDialog.close();
      return;
    }

    this.isRenameInProgress_ = true;

    // The C++ layer uses std::u16string, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted
    // to an array of 16 bit character codes that match std::u16string.
    const name = {data: Array.from(this.esimProfileName_, c => c.charCodeAt())};

    this.esimProfileRemote_.setProfileNickname(name).then(response => {
      this.handleSetProfileNicknameResponse_(response.result);
    });
  },

  /**
   * @param {chromeos.cellularSetup.mojom.ESimOperationResult} result
   * @private
   */
  handleSetProfileNicknameResponse_(result) {
    this.isRenameInProgress_ = false;
    if (result === chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
      this.errorMessage_ = this.i18n('eSimRenameProfileDialogError');
      return;
    }
    this.$.profileRenameDialog.close();
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.profileRenameDialog.close();
  },
});
