// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element used to display Bluetooth device icon.
 */

import './bluetooth_icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {BluetoothDeviceProperties, DeviceType} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_icon.html.js';
import {hasDefaultImage} from './bluetooth_utils.js';

export class SettingsBluetoothIconElement extends PolymerElement {
  static get is() {
    return 'bluetooth-icon' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: {
        type: Object,
      },
    };
  }

  device: BluetoothDeviceProperties;

  private getIcon_(): string {
    if (!this.device) {
      return 'default';
    }

    switch (this.device.deviceType) {
      case DeviceType.kComputer:
        return 'computer';
      case DeviceType.kPhone:
        return 'phone';
      case DeviceType.kHeadset:
        return 'headset';
      case DeviceType.kVideoCamera:
        return 'video-camera';
      case DeviceType.kGameController:
        return 'game-controller';
      case DeviceType.kKeyboard:
      case DeviceType.kKeyboardMouseCombo:
        return 'keyboard';
      case DeviceType.kMouse:
        return 'mouse';
      case DeviceType.kTablet:
        return 'tablet';
      default:
        return 'default';
    }
  }

  private hasDefaultImage_(): boolean {
    return hasDefaultImage(this.device);
  }

  private getDefaultImageSrc_(): string {
    if (!this.hasDefaultImage_()) {
      return '';
    }
    return this.device!.imageInfo!.defaultImageUrl!.url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothIconElement.is]: SettingsBluetoothIconElement;
  }
}

customElements.define(
    SettingsBluetoothIconElement.is, SettingsBluetoothIconElement);
