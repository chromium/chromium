// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-paired_bluetooth-list> that displays information for a paired
 * Bluetooth device.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../os_icons.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsPairedBluetoothListItemElement extends PolymerElement {
  static get is() {
    return 'os-settings-paired-bluetooth-list-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsPairedBluetoothListItemElement.is,
    SettingsPairedBluetoothListItemElement);