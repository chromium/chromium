// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Bluetooth properties and devices. This page
 * provides a high-level summary and routing to subpages
 */

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../../settings_shared_css.js';
import '../../settings_page/settings_animated_pages.js';
import './os_bluetooth_devices_subpage.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Router} from '../../router.js';
import {routes} from '../os_route.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const SettingsBluetoothPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

class SettingsBluetoothPageElement extends SettingsBluetoothPageElementBase {
  static get is() {
    return 'os-settings-bluetooth-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @private */
  onSubpageArrowTap_() {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
  }
}

customElements.define(
    SettingsBluetoothPageElement.is, SettingsBluetoothPageElement);
