// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-saved-devices-list> that displays information for a saved
 * Bluetooth device.
 */

import '../settings_shared.css.js';
import '../os_settings_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';

import {FastPairSavedDevicesUiEvent, recordSavedDevicesUiEventMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {FocusRowMixin} from 'chrome://resources/ash/common/cr_elements/focus_row_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_saved_devices_list_item.html.js';
import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

interface SettingsSavedDevicesListItemElement {
  $: {
    dotsMenu: CrActionMenuElement,
  };
}

const SettingsSavedDevicesListItemElementBase =
    FocusRowMixin(WebUiListenerMixin(PolymerElement));

class SettingsSavedDevicesListItemElement extends
    SettingsSavedDevicesListItemElementBase {
  static get is() {
    return 'os-settings-saved-devices-list-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: {
        type: Object,
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      shouldShowRemoveSavedDeviceDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  device: FastPairSavedDevice;
  itemIndex: number;
  listSize: number;
  private shouldShowRemoveSavedDeviceDialog_: boolean;

  private getDeviceNameUnsafe_(device: FastPairSavedDevice): string {
    return device.name;
  }

  private getImageSrc_(device: FastPairSavedDevice): string {
    return device.imageUrl;
  }

  private onMenuButtonClick_(event: Event): void {
    const button = event.target as HTMLElement;
    this.$.dotsMenu.showAt(button);
    event.stopPropagation();
  }

  private onRemoveClick_(): void {
    recordSavedDevicesUiEventMetrics(
        FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_REMOVE_DIALOG);
    this.shouldShowRemoveSavedDeviceDialog_ = true;
    this.$.dotsMenu.close();
  }

  private onCloseRemoveDeviceDialog_(): void {
    this.shouldShowRemoveSavedDeviceDialog_ = false;
  }

  private getAriaLabel_(device: FastPairSavedDevice): string {
    const deviceName = this.getDeviceNameUnsafe_(device);
    return loadTimeData.getStringF(
        'savedDeviceItemA11yLabel', this.itemIndex + 1, this.listSize,
        deviceName);
  }

  private getSubpageButtonA11yLabel_(device: FastPairSavedDevice): string {
    const deviceName = this.getDeviceNameUnsafe_(device);
    return loadTimeData.getStringF(
        'savedDeviceItemButtonA11yLabel', deviceName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSavedDevicesListItemElement.is]:
        SettingsSavedDevicesListItemElement;
  }
}

customElements.define(
    SettingsSavedDevicesListItemElement.is,
    SettingsSavedDevicesListItemElement);
