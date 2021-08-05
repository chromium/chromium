// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth device detail.
 */

import '../../settings_shared_css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsBluetoothDeviceDetailSubpageElement extends PolymerElement {
  static get is() {
    return 'os-settings-bluetooth-device-detail-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsBluetoothDeviceDetailSubpageElement.is,
    SettingsBluetoothDeviceDetailSubpageElement);