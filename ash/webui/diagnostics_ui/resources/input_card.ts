// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_card_frame.js';
import './icons.html.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConnectionType, KeyboardInfo} from './input.mojom-webui.js';
import {getTemplate} from './input_card.html.js';
import {InputDataProviderInterface, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {HostDeviceStatus} from './input_list.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

declare global {
  interface HTMLElementEventMap {
    'test-button-click': CustomEvent<{evdevId: number}>;
  }
}

/**
 * @fileoverview
 * 'input-card' is responsible for displaying a list of input devices with links
 * to their testers.
 */

/**
 * Enum of device types supported by input-card elements.
 */
export enum InputCardType {
  KEYBOARD = 'keyboard',
  TOUCHPAD = 'touchpad',
  TOUCHSCREEN = 'touchscreen',
}

const InputCardElementBase = I18nMixin(PolymerElement);

export class InputCardElement extends InputCardElementBase {
  static get is(): string {
    return 'input-card';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The type of input device to be displayed. Valid values are 'keyboard',
       * 'touchpad', and 'touchscreen'.
       */
      deviceType: String,

      devices: {
        type: Array,
        value: () => [],
      },

      deviceIcon: {
        type: String,
        computed: 'computeDeviceIcon(deviceType)',
      },

      hostDeviceStatus: {
        type: Object,
      },
    };
  }

  deviceType: InputCardType;
  devices: KeyboardInfo[]|TouchDeviceInfo[];
  hostDeviceStatus: HostDeviceStatus;

  private deviceIcon: string;
  private inputDataProvider: InputDataProviderInterface =
      getInputDataProvider();

  private computeDeviceIcon(deviceType: InputCardType): string {
    return {
      [InputCardType.KEYBOARD]: 'diagnostics:keyboard',
      [InputCardType.TOUCHPAD]: 'diagnostics:touchpad',
      [InputCardType.TOUCHSCREEN]: 'diagnostics:touchscreen',
    }[deviceType];
  }

  /**
   * Fetches the description string for a device based on its connection type
   * (e.g. "Bluetooth keyboard", "Internal touchpad").
   */
  private getDeviceDescription(device: KeyboardInfo|TouchDeviceInfo): string {
    if (device.connectionType === ConnectionType.kUnknown) {
      return '';
    }
    const connectionTypeString = {
      [ConnectionType.kInternal]: 'Internal',
      [ConnectionType.kUsb]: 'Usb',
      [ConnectionType.kBluetooth]: 'Bluetooth',
    }[device.connectionType];
    const deviceTypeString = {
      [InputCardType.KEYBOARD]: 'Keyboard',
      [InputCardType.TOUCHPAD]: 'Touchpad',
      [InputCardType.TOUCHSCREEN]: 'Touchscreen',
    }[this.deviceType];
    return loadTimeData.getString(
        'inputDescription' + connectionTypeString + deviceTypeString);
  }

  private isInternalKeyboard(device: KeyboardInfo|TouchDeviceInfo): boolean {
    return this.deviceType == InputCardType.KEYBOARD &&
        device.connectionType == ConnectionType.kInternal;
  }

  private isInternalKeyboardTestable(): boolean {
    return !this.hostDeviceStatus.isTabletMode &&
        this.hostDeviceStatus.isLidOpen;
  }

  /**
   * Grey out the test button if the test device is untestable. e.g. if the
   * laptop's lid is closed, the internal touchscreen is untestable.
   */
  private getDeviceTestability(device: KeyboardInfo|TouchDeviceInfo): boolean {
    // If the device has the key 'testable', we check its testable state.
    if ('testable' in device) {
      return (device as TouchDeviceInfo).testable;
    }

    if (this.isInternalKeyboard(device)) {
      return this.isInternalKeyboardTestable();
    }

    return true;
  }

  private getDeviceTestabilityErrorMessage(device: KeyboardInfo|
                                           TouchDeviceInfo): string {
    // If it is not an internal keyboard, return the generic untestable string.
    if (!this.isInternalKeyboard(device)) {
      return loadTimeData.getString('inputDeviceUntestableNote');
    }

    // Otherwise, differentiate the string based on the reason it is untestable.
    if (this.hostDeviceStatus.isTabletMode) {
      return loadTimeData.getString('inputKeyboardUntestableTabletModeNote');
    }

    if (!this.hostDeviceStatus.isLidOpen) {
      return loadTimeData.getString('inputKeyboardUntestableLidClosedNote');
    }

    // Otherwise, there is no error.
    return '';
  }

  private handleTestButtonClick(e: PointerEvent): void {
    const inputDeviceButton = e.target as CrButtonElement;
    assert(inputDeviceButton);
    const closestDevice: HTMLDivElement|null =
        inputDeviceButton.closest('.device');
    assert(closestDevice);
    const dataEvdevId = closestDevice.getAttribute('data-evdev-id');
    assert(typeof dataEvdevId === 'string');
    const evdevId = parseInt(dataEvdevId, 10);
    this.dispatchEvent(new CustomEvent(
        'test-button-click', {composed: true, detail: {evdevId: evdevId}}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-card': InputCardElement;
  }
}

customElements.define(InputCardElement.is, InputCardElement);
