// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to remove eSIM profile
 */

import '//resources/cr_components/chromeos/cellular_setup/cellular_setup_icons.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';

import {getESimProfile, getESimProfileProperties, getEuicc, getNonPendingESimProfiles, getNumESimProfiles, getPendingESimProfiles} from '//resources/cr_components/chromeos/cellular_setup/esim_manager_utils.m.js';
import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
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

  /** @private {?ash.cellularSetup.mojom.ESimProfileRemote} */
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
    this.esimProfileRemote_ =
        await getESimProfile(this.networkState.typeState.cellular.iccid);
    // Fail gracefully if init is incomplete, see crbug/1194729.
    if (!this.esimProfileRemote_) {
      this.fire('show-error-toast', this.i18n('eSimRemoveProfileDialogError'));
      this.$.dialog.close();
      return;
    }
    this.esimProfileName_ = this.networkState.name;
    this.$.cancel.focus();
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
    if (!this.esimProfileName_) {
      return '';
    }
    return this.i18n('esimRemoveProfileDialogTitle', this.esimProfileName_);
  },

  /**
   * @param {Event} event
   * @private
   */
  onRemoveProfileTap_(event) {
    this.esimProfileRemote_.uninstallProfile().then((response) => {
      if (response.result ===
          ash.cellularSetup.mojom.ESimOperationResult.kFailure) {
        this.fire(
            'show-error-toast', this.i18n('eSimRemoveProfileDialogError'));
      }
    });
    this.$.dialog.close();
    const params = new URLSearchParams();
    params.append(
        'type',
        OncMojo.getNetworkTypeString(
            chromeos.networkConfig.mojom.NetworkType.kCellular));
    Router.getInstance().setCurrentRoute(
        routes.INTERNET_NETWORKS, params, /*isPopState=*/ true);
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.dialog.close();
  },

  /**
   * @param {string} esimProfileName
   * @return {string}
   * @private
   */
  getRemoveBtnA11yLabel_(esimProfileName) {
    return this.i18n('eSimRemoveProfileRemoveA11yLabel', esimProfileName);
  },

  /**
   * @param {string} esimProfileName
   * @return {string}
   * @private
   */
  getCancelBtnA11yLabel_(esimProfileName) {
    return this.i18n('eSimRemoveProfileCancelA11yLabel', esimProfileName);
  }
});
