// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying saved Bluetooth devices.
 */

import '../settings_shared.css.js';
import './os_saved_devices_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {getTemplate} from './os_saved_devices_list.html.js';
import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

declare global {
  interface HTMLElementEventMap {
    'remove-saved-device': CustomEvent<{key: string}>;
  }
}

const SettingsSavedDevicesListElementBase =
    CrScrollableMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsSavedDevicesListElement extends
    SettingsSavedDevicesListElementBase {
  static get is() {
    return 'os-settings-saved-devices-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      devices: {
        type: Array,
        observer: 'onDevicesChanged_',
        value: [],
      },

      /**
       * Used by FocusRowMixin in <os-settings-saved-devices-list-item>
       * to track the last focused element on a row.
       */
      lastFocused_: Object,
    };
  }

  devices: FastPairSavedDevice[];
  private browserProxy_: OsBluetoothDevicesSubpageBrowserProxy;
  private lastFocused_: HTMLElement;

  constructor() {
    super();

    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('remove-saved-device', this.removeSavedDevice_);
  }

  private removeSavedDevice_(event: CustomEvent<{key: string}>): void {
    this.browserProxy_.deleteFastPairSavedDevice(event.detail.key);
    for (let i = 0; i < this.devices.length; i++) {
      if (this.devices[i].accountKey === event.detail.key) {
        this.splice('devices', i, 1);
        break;
      }
    }
    this.updateScrollableContents();
  }

  private onDevicesChanged_(): void {
    // CrScrollableMixin method required for list items to be
    // properly rendered when devices updates.
    this.updateScrollableContents();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSavedDevicesListElement.is]: SettingsSavedDevicesListElement;
  }
}

customElements.define(
    SettingsSavedDevicesListElement.is, SettingsSavedDevicesListElement);
