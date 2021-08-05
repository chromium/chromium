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
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsPairedBluetoothListElement extends PolymerElement {
  static get is() {
    return 'os-settings-paired-bluetooth-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * TODO(crbug.com/1010321): Use actual Device objects.
       * @private {Array<Object>}
       */
      devices_: {
        type: Array,
        value() {
          return [{}, {}, {}];
        }
      }
    };
  }
}

customElements.define(
    SettingsPairedBluetoothListElement.is, SettingsPairedBluetoothListElement);