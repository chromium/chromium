// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'touchpad-settings' allow users to configure their touchpad settings in
 * system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {Touchpad} from './input_device_settings_types.js';
import {getDeviceStateChangesToAnnounce} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_touchpad.html.js';

const SettingsPerDeviceTouchpadElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsPerDeviceTouchpadElement extends
    SettingsPerDeviceTouchpadElementBase {
  static get is() {
    return 'settings-per-device-touchpad';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      touchpads: {
        type: Array,
        observer: 'onTouchpadListUpdated',
      },
    };
  }

  protected touchpads: Touchpad[];

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_TOUCHPAD) {
      return;
    }
  }

  private onTouchpadListUpdated(
      newTouchpadList: Touchpad[],
      oldTouchpadList: Touchpad[]|undefined): void {
    if (!oldTouchpadList) {
      return;
    }
    const {msgId, deviceNames} =
        getDeviceStateChangesToAnnounce(newTouchpadList, oldTouchpadList);
    for (const deviceName of deviceNames) {
      getAnnouncerInstance().announce(this.i18n(msgId, deviceName));
    }
  }

  private computeIsLastDevice(index: number): boolean {
    return index === this.touchpads.length - 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-touchpad': SettingsPerDeviceTouchpadElement;
  }
}

customElements.define(
    SettingsPerDeviceTouchpadElement.is, SettingsPerDeviceTouchpadElement);
