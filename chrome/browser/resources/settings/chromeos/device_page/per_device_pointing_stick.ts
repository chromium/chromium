// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pointing-stick-settings' allow users to configure their pointing stick
 * settings in system settings.
 */

import '../../icons.html.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, PointingStick} from './input_device_settings_types.js';
import {getTemplate} from './per_device_pointing_stick.html.js';

const SettingsPerDevicePointingStickElementBase =
    RouteObserverMixin(PolymerElement);

class SettingsPerDevicePointingStickElement extends
    SettingsPerDevicePointingStickElementBase {
  static get is(): string {
    return 'settings-per-device-pointing-stick';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      pointingSticks: {
        type: Array,
      },
    };
  }

  protected pointingSticks: PointingStick[];
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

  constructor() {
    super();
    this.fetchConnectedPointingSticks();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_POINTING_STICK) {
      return;
    }
  }

  private async fetchConnectedPointingSticks(): Promise<void> {
    this.pointingSticks = await this.inputDeviceSettingsProvider
                              .getConnectedPointingStickSettings();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-pointing-stick': SettingsPerDevicePointingStickElement;
  }
}

customElements.define(
    SettingsPerDevicePointingStickElement.is,
    SettingsPerDevicePointingStickElement);
