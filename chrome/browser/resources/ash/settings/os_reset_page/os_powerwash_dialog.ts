// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog' is a dialog shown to request confirmation
 * from the user for a device reset (aka powerwash).
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../settings_shared.css.js';
import './os_powerwash_dialog_esim_item.js';

import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './os_powerwash_dialog.html.js';
import {OsResetBrowserProxy, OsResetBrowserProxyImpl} from './os_reset_browser_proxy.js';

export interface OsSettingsPowerwashDialogElement {
  $: {
    cancel: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class OsSettingsPowerwashDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-powerwash-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      requestTpmFirmwareUpdate: {
        type: Boolean,
        value: false,
      },

      installedESimProfiles: {
        type: Array,
        value() {
          return [];
        },
      },

      shouldShowESimWarning_: {
        type: Boolean,
        value: false,
        computed:
            'computeShouldShowEsimWarning_(installedESimProfiles, hasContinueBeenTapped_)',
      },

      isESimCheckboxChecked_: {
        type: Boolean,
        value: false,
      },

      hasContinueBeenTapped_: {
        type: Boolean,
        value: false,
      },
    };
  }

  installedESimProfiles: ESimProfileRemote[];
  requestTpmFirmwareUpdate: boolean;
  private hasContinueBeenTapped_: boolean;
  private isESimCheckboxChecked_: boolean;
  private lifetimeBrowserProxy_: LifetimeBrowserProxy;
  private osResetBrowserProxy_: OsResetBrowserProxy;
  private shouldShowESimWarning_: boolean;

  constructor() {
    super();

    this.osResetBrowserProxy_ = OsResetBrowserProxyImpl.getInstance();
    this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onRestartClick_(): void {
    recordSettingChange(Setting.kPowerwash);
    LifetimeBrowserProxyImpl.getInstance().factoryReset(
        this.requestTpmFirmwareUpdate);
  }

  private onContinueClick_(): void {
    this.hasContinueBeenTapped_ = true;
  }

  private onMobileSettingsLinkClicked_(event: CustomEvent): void {
    event.detail.event.preventDefault();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

    this.$.dialog.close();
  }

  private computeShouldShowEsimWarning_(): boolean {
    if (this.hasContinueBeenTapped_) {
      return false;
    }
    return !!this.installedESimProfiles.length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-powerwash-dialog': OsSettingsPowerwashDialogElement;
  }
}

customElements.define(
    OsSettingsPowerwashDialogElement.is, OsSettingsPowerwashDialogElement);
