// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-subsection-header' displays information about a device, with
 * conditional layout based on the 'isWelcomeExperienceEnabled' flag.
 * - When enabled: Shows device image (if available), name, and optional battery
 * info.
 * - When disabled: Shows device name only.
 */

import './input_device_settings_shared.css.js';
import '../icons.html.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_battery_icon_percentage.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {BatteryInfo, InputDeviceSettingsProviderInterface} from './input_device_settings_types.js';
import {createBluetoothDeviceProperties} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_subsection_header.html.js';

// Defines the possible states for a device's display.
enum DeviceDisplayState {
  FETCHING_IMAGE = 0,
  IMAGE_AVAILABLE = 1,
  IMAGE_UNAVAILABLE = 2,
}

const PerDeviceSubsectionHeaderElementBase = I18nMixin(PolymerElement);

export class PerDeviceSubsectionHeaderElement extends
    PerDeviceSubsectionHeaderElementBase {
  static get is() {
    return 'per-device-subsection-header' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      isWelcomeExperienceEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableWelcomeExperience');
        },
        readOnly: true,
      },

      deviceImageDataUrl: {
        type: String,
        value: '',
      },

      batteryInfo: {
        type: Object,
      },

      bluetoothDevice: {
        type: Object,
        computed: 'computeBluetoothDeviceProperties(batteryInfo.*)',
      },

      deviceKey: {
        type: String,
      },

      name: {
        type: String,
      },

      icon: {
        type: String,
      },

      deviceDisplayState: {
        type: Number,
        value: DeviceDisplayState.FETCHING_IMAGE,
      },
    };
  }

  isWelcomeExperienceEnabled: boolean;
  deviceImageDataUrl: string|null = null;
  deviceKey: string;
  deviceDisplayState: DeviceDisplayState;
  name: string;
  bluetoothDevice: BluetoothDeviceProperties|null;
  batteryInfo: BatteryInfo|null;
  icon: string;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

  static get observers() {
    return [
      'handleDeviceKeyChange(deviceKey)',
    ];
  }

  showBatteryInfo(): boolean {
    return !!this.batteryInfo;
  }

  getDefaultBatteryType(): BatteryType {
    return BatteryType.DEFAULT;
  }

  computeBluetoothDeviceProperties(): BluetoothDeviceProperties|null {
    if (!this.batteryInfo) {
      return null;
    }

    return createBluetoothDeviceProperties(
        this.deviceKey, this.name, this?.batteryInfo.batteryPercentage);
  }

  async handleDeviceKeyChange(): Promise<void> {
    if (this.isWelcomeExperienceEnabled) {
      this.deviceDisplayState = DeviceDisplayState.FETCHING_IMAGE;
      this.deviceImageDataUrl =
          (await this.inputDeviceSettingsProvider.getDeviceIconImage(
               this.deviceKey))
              ?.dataUrl;
      this.deviceDisplayState = this.deviceImageDataUrl ?
          DeviceDisplayState.IMAGE_AVAILABLE :
          DeviceDisplayState.IMAGE_UNAVAILABLE;
    }
  }

  shouldShowPlaceholder(): boolean {
    return this.deviceDisplayState === DeviceDisplayState.FETCHING_IMAGE;
  }

  shouldShowDeviceIcon(): boolean {
    return this.deviceDisplayState === DeviceDisplayState.IMAGE_UNAVAILABLE;
  }

  getAriaLabel(): string {
    let label = `${this.i18n('deviceNameLabel')} ${this.name}`;
    if (this.batteryInfo?.batteryPercentage) {
      label += ` ${
          this.i18n(
              'deviceBatteryLabel', this?.batteryInfo.batteryPercentage)}`;
    }
    return label;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerDeviceSubsectionHeaderElement.is]: PerDeviceSubsectionHeaderElement;
  }
}

customElements.define(
    PerDeviceSubsectionHeaderElement.is, PerDeviceSubsectionHeaderElement);
