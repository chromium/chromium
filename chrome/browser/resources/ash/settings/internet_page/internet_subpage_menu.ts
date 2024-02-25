// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-internet-subpage-menu' is a menu that provides
 * additional technology specific actions for a network type in the network
 * subpage.
 */
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './network_device_info_dialog.js';

import {ESimManagerListenerMixin} from 'chrome://resources/ash/common/cellular_setup/esim_manager_listener_mixin.js';
import {getEuicc} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {EuiccRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './internet_subpage_menu.html.js';

const SettingsInternetSubpageMenuElementBase =
    ESimManagerListenerMixin(WebUiListenerMixin(PolymerElement));

export class SettingsInternetSubpageMenuElement extends
    SettingsInternetSubpageMenuElementBase {
  static get is() {
    return 'settings-internet-subpage-menu' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Device state for the network type
       */
      deviceState: Object,

      showDeviceNetworkInfoDialog_: {type: Boolean, value: false},
    };
  }

  deviceState: OncMojo.DeviceStateProperties|undefined;

  private showDeviceNetworkInfoDialog_: boolean;
  private euicc_: EuiccRemote|null;

  constructor() {
    super();

    this.fetchEuicc_();
  }

  override onAvailableEuiccListChanged(): void {
    this.fetchEuicc_();
  }

  private async fetchEuicc_(): Promise<void> {
    const euicc = await getEuicc();
    this.euicc_ = euicc;
  }

  private shouldShowDotsMenuButton_(): boolean {
    const isCellularSubpage = this.deviceState?.type === NetworkType.kCellular;
    return isCellularSubpage && (!!this.euicc_ || !!this.deviceState?.imei);
  }

  private onShowDeviceInfoClick_(): void {
    this.closeMenu_();
    this.showDeviceNetworkInfoDialog_ = true;
  }

  private onCloseDeviceNetworkInfoDialog_(): void {
    this.showDeviceNetworkInfoDialog_ = false;
  }

  private closeMenu_(): void {
    const actionMenu =
        castExists(this.shadowRoot!.querySelector('cr-action-menu'));
    actionMenu.close();
  }

  private onDotsClick_(e: Event): void {
    const menu = this.shadowRoot!
                     .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                         '#menu')!.get();
    menu.showAt(e.target as HTMLElement);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetSubpageMenuElement.is]: SettingsInternetSubpageMenuElement;
  }
}

customElements.define(
    SettingsInternetSubpageMenuElement.is, SettingsInternetSubpageMenuElement);