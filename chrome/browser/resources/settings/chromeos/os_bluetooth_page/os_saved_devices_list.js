// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying saved Bluetooth devices.
 */

import '../../settings_shared.css.js';
import './os_saved_devices_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/cr_elements/cr_scrollable_behavior.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
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
       * @protected {Array<!FastPairSavedDevice>}
       */
      devices_: {
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
    for (let i = 0; i < this.devices_.length; i++) {
      if (this.devices_[i].accountKey === event.detail.key) {
        this.devices_.splice(i, 1);
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
