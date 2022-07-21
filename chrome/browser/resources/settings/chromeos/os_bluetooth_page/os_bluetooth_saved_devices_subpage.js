// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth saved devices.
 */

import '../../settings_shared.css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/** @polymer */
class SettingsBluetoothSavedDevicesSubpageElement extends PolymerElement {
  static get is() {
    return 'os-settings-bluetooth-saved-devices-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  constructor() {
    super();
    this.parentNode.pageTitle = loadTimeData.getString('savedDevicesPageName');
  }
}

customElements.define(
    SettingsBluetoothSavedDevicesSubpageElement.is,
    SettingsBluetoothSavedDevicesSubpageElement);
