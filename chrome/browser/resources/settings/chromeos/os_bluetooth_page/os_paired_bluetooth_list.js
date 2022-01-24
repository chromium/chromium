// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying paired Bluetooth devices.
 */

import '../../settings_shared_css.js';
import './os_paired_bluetooth_list_item.js';

import '//resources/polymer/v3_0/iron-list/iron-list.js';
import {CrScrollableBehavior, CrScrollableBehaviorInterface} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 */
const SettingsPairedBluetoothListElementBase =
    mixinBehaviors([CrScrollableBehavior], PolymerElement);

/** @polymer */
class SettingsPairedBluetoothListElement extends
    SettingsPairedBluetoothListElementBase {
  static get is() {
    return 'os-settings-paired-bluetooth-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {!Array<!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
       */
      devices: {
        type: Array,
        observer: 'onDevicesChanged_',
        value: [],
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       * @private
       */
      lastFocused_: Object,
    };
  }

  /** @private */
  onDevicesChanged_() {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates.
    this.updateScrollableContents();
  }
}

customElements.define(
    SettingsPairedBluetoothListElement.is, SettingsPairedBluetoothListElement);