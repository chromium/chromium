// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-settings-remap-keys' displays the remapped keys and
 * allow users to configure their keyboard remapped keys for each keyboard.
 */

import '../../icons.html.js';
import '../../settings_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {Keyboard} from './input_device_settings_types.js';
import {getTemplate} from './per_device_keyboard_remap_keys.html.js';

const SettingsPerDeviceKeyboardRemapKeysElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

export class SettingsPerDeviceKeyboardRemapKeysElement extends
    SettingsPerDeviceKeyboardRemapKeysElementBase {
  static get is(): string {
    return 'settings-per-device-keyboard-remap-keys';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      keyboardId: String,

      keyboard: {
        type: Object,
      },
    };
  }

  protected keyboard: Keyboard;
  // This variable is temporary and only to verify the remapping subpage has
  // receive the correct keyboardId. Will remove it from properties after
  // implementing getKeyboard() method.
  private keyboardId: string;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
      return;
    }
    this.getKeyboard();
  }

  private getKeyboard(): void {
    const keyboardId =
        Router.getInstance().getQueryParameters().get('keyboardId');
    assert(keyboardId);
    this.keyboardId = keyboardId;
    // TODO(yyhyyh@): Get the keyboard object using keyboardId and remove
    // variable "this.keyboardId".
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard-remap-keys':
        SettingsPerDeviceKeyboardRemapKeysElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardRemapKeysElement.is,
    SettingsPerDeviceKeyboardRemapKeysElement);
