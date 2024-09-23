// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Bluetooth properties and devices. This page
 * provides a high-level summary and routing to subpages
 */

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import './os_bluetooth_devices_subpage.js';
import './os_bluetooth_summary.js';
import './os_bluetooth_device_detail_subpage.js';
import './os_bluetooth_pairing_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {BluetoothSystemProperties, BluetoothSystemState, SystemPropertiesObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {getTemplate} from './os_bluetooth_page.html.js';

const SettingsBluetoothPageElementBase = PrefsMixin(I18nMixin(PolymerElement));

export class SettingsBluetoothPageElement extends
    SettingsBluetoothPageElementBase {
  static get is() {
    return 'os-settings-bluetooth-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kBluetooth,
        readOnly: true,
      },

      systemProperties_: Object,

      shouldShowPairingDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Set by Saved Devices subpage. Controls spinner and loading label
       * visibility in the subpage.
       */
      showSavedDevicesLoadingIndicators_: Boolean,

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },
    };
  }

  private browserProxy_: OsBluetoothDevicesSubpageBrowserProxy;
  private section_: Section;
  private showSavedDevicesLoadingIndicators_: boolean;
  private shouldShowPairingDialog_: boolean;
  private systemProperties_: BluetoothSystemProperties;
  private systemPropertiesObserverReceiver_: SystemPropertiesObserverReceiver;
  private isRevampWayfindingEnabled_: boolean;

  constructor() {
    super();

    this.systemPropertiesObserverReceiver_ =
        new SystemPropertiesObserverReceiver(this);
    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    getBluetoothConfig().observeSystemProperties(
        this.systemPropertiesObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * SystemPropertiesObserverInterface override
   */
  onPropertiesUpdated(properties: BluetoothSystemProperties): void {
    this.systemProperties_ = properties;
  }

  private onStartPairing_(): void {
    this.shouldShowPairingDialog_ = true;
    this.browserProxy_.showBluetoothRevampHatsSurvey();
  }

  private onClosePairingDialog_(): void {
    this.shouldShowPairingDialog_ = false;
  }

  private shouldShowPairNewDevice_(): boolean {
    if (!this.systemProperties_) {
      return false;
    }

    return this.systemProperties_.systemState === BluetoothSystemState.kEnabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPageElement.is]: SettingsBluetoothPageElement;
  }
}

customElements.define(
    SettingsBluetoothPageElement.is, SettingsBluetoothPageElement);
