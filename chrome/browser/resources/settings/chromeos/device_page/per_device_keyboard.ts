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
import {Route, Router} from '../router.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, Keyboard} from './input_device_settings_types.js';
import {getTemplate} from './per_device_keyboard.html.js';

const SettingsPerDeviceKeyboardElementBase = RouteObserverMixin(PolymerElement);

export class SettingsPerDeviceKeyboardElement extends
    SettingsPerDeviceKeyboardElementBase {
  static get is(): string {
    return 'settings-per-device-keyboard';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      keyboards: {
        type: Array,
      },
    };
  }

  protected keyboards: Keyboard[];
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private browserProxy: DevicePageBrowserProxy =
      DevicePageBrowserProxyImpl.getInstance();

  constructor() {
    super();
    this.fetchConnectedKeyboards();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy.initializeKeyboard();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD) {
      return;
    }
  }

  private async fetchConnectedKeyboards(): Promise<void> {
    this.keyboards =
        await this.inputDeviceSettingsProvider.getConnectedKeyboardSettings();
  }

  private onShowKeyboardShortcutViewerTap(): void {
    this.browserProxy.showKeyboardShortcutViewer();
  }

  private onShowInputSettingsTap(): void {
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT,
        /*dynamicParams=*/ undefined, /*removeSearch=*/ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard': SettingsPerDeviceKeyboardElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardElement.is, SettingsPerDeviceKeyboardElement);
