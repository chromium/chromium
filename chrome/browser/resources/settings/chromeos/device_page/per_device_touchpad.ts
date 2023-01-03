// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'touchpad-settings' allow users to configure their touchpad settings in
 * system settings.
 */

import '../../icons.html.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './per_device_touchpad.html.js';

const SettingsPerDeviceTouchpadElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

class SettingsPerDeviceTouchpadElement extends
    SettingsPerDeviceTouchpadElementBase {
  static get is() {
    return 'settings-per-device-touchpad';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isDeviceSettingsSplitEnabled: {
        type: Boolean,
        value: false,
      },
    };
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_TOUCHPAD) {
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-touchpad': SettingsPerDeviceTouchpadElement;
  }
}

customElements.define(
    SettingsPerDeviceTouchpadElement.is, SettingsPerDeviceTouchpadElement);
