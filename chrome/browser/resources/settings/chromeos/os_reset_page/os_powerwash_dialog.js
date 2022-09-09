// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog' is a dialog shown to request confirmation
 * from the user for a device reset (aka powerwash).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';
import './os_powerwash_dialog_esim_item.js';

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxyImpl} from '../../lifetime_browser_proxy.js';
import {Router} from '../../router.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';

import {OsResetBrowserProxy, OsResetBrowserProxyImpl} from './os_reset_browser_proxy.js';

/** @polymer */
class OsSettingsPowerwashDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-powerwash-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      requestTpmFirmwareUpdate: {
        type: Boolean,
        value: false,
      },

      /**
       * @type {!Array<!ash.cellularSetup.mojom.ESimProfileRemote>}
       * @private
       */
      installedESimProfiles: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @private */
      shouldShowESimWarning_: {
        type: Boolean,
        value: false,
        computed:
            'computeShouldShowESimWarning_(installedESimProfiles, hasContinueBeenTapped_)',
      },

      /** @private */
      isESimCheckboxChecked_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      hasContinueBeenTapped_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {!OsResetBrowserProxy} */
    this.osResetBrowserProxy_ = OsResetBrowserProxyImpl.getInstance();

    /** @private */
    this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.osResetBrowserProxy_.onPowerwashDialogShow();
    this.$.dialog.showModal();
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onRestartTap_() {
    recordSettingChange();
    LifetimeBrowserProxyImpl.getInstance().factoryReset(
        this.requestTpmFirmwareUpdate);
  }

  /** @private */
  onContinueTap_() {
    this.hasContinueBeenTapped_ = true;
  }

  /** @private */
  onMobileSettingsLinkClicked_(event) {
    event.detail.event.preventDefault();

    const params = new URLSearchParams();
    params.append(
        'type',
        OncMojo.getNetworkTypeString(
            chromeos.networkConfig.mojom.NetworkType.kCellular));
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

    this.$.dialog.close();
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowESimWarning_() {
    if (this.hasContinueBeenTapped_) {
      return false;
    }
    return !!this.installedESimProfiles.length;
  }
}

customElements.define(
    OsSettingsPowerwashDialogElement.is, OsSettingsPowerwashDialogElement);
