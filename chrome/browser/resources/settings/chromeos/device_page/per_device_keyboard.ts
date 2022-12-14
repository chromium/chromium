// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-settings' allow users to configure their keyboard
 * settings for each device in system settings.
 */

import '../../icons.html.js';
import '../../settings_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {routes} from '../os_route.js';
import {Route} from '../router.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';

import {getTemplate} from './per_device_keyboard.html.js';

const SettingsPerDeviceKeyboardElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

class SettingsPerDeviceKeyboardElement extends
    SettingsPerDeviceKeyboardElementBase {
  static get is() {
    return 'settings-per-device-keyboard';
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

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD) {
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard': SettingsPerDeviceKeyboardElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardElement.is, SettingsPerDeviceKeyboardElement);