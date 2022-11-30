// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying saved Bluetooth devices.
 */

import '../../settings_shared.css.js';
import './os_saved_devices_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsSavedDevicesListElementBase = mixinBehaviors(
    [CrScrollableBehavior, WebUIListenerBehavior, I18nBehavior],
    PolymerElement);

/** @polymer */
class SettingsSavedDevicesListElement extends
    SettingsSavedDevicesListElementBase {
  static get is() {
    return 'os-settings-saved-devices-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @public {Array<!FastPairSavedDevice>}
       */
      devices: {
        type: Array,
        observer: 'onDevicesChanged_',
        value: [],
      },
    };
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('remove-saved-device', this.removeSavedDevice_);
  }

  constructor() {
    super();
    /** @private {?OsBluetoothDevicesSubpageBrowserProxy} */
    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  /**
   * @param {!Event} event
   * @private
   */
  removeSavedDevice_(/** @type {CustomEvent} */ event) {
    this.browserProxy_.deleteFastPairSavedDevice(event.detail.key);
    for (let i = 0; i < this.devices.length; i++) {
      if (this.devices[i].accountKey === event.detail.key) {
        this.splice('devices', i, 1);
        break;
      }
    }
    this.updateScrollableContents();
  }

  /** @private */
  onDevicesChanged_() {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates.
    this.updateScrollableContents();
  }
}

customElements.define(
    SettingsSavedDevicesListElement.is, SettingsSavedDevicesListElement);
