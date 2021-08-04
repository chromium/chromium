// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth properties and devices.
 */

import '../../settings_shared_css.js';
import './os_paired_bluetooth_list.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothDevicesSubpageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothDevicesSubpageElement extends
    SettingsBluetoothDevicesSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-devices-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);