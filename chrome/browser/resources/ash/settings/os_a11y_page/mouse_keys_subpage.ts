// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {routes} from '../router.js';

import {getTemplate} from './mouse_keys_subpage.html.js';

interface TickData {
  tick: number;
  percent: number;
  defaultValue: number;
}

interface SliderData {
  min: number;
  max: number;
  step: number;
  defaultValue: number;
}

const SettingsMouseKeysSubpageElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface SettingsMouseKeysSubpageElement {}

export class SettingsMouseKeysSubpageElement extends
    SettingsMouseKeysSubpageElementBase {
  static get is() {
    return 'settings-mouse-keys-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      toggleLabel_: {
        type: String,
        computed:
            'getToggleLabel_(prefs.settings.a11y.mouse_keys.enabled.value)',
      },

      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kMouseKeysEnabled,
        ]),
      },

      mouseKeysDominantHandOptions_: {
        readOnly: true,
        type: Array,
        value() {
          // These values correspond to the values of MouseKeysDominantHand in
          // ash/public/cpp/accessibility_controller_enums.h
          // If these values get changed then this needs to be updated as well.
          return [
            {value: 1, name: loadTimeData.getString('mouseKeysLeftHand')},
            {value: 0, name: loadTimeData.getString('mouseKeysRightHand')},
          ];
        },
      },
    };
  }

  private getToggleLabel_(): string {
    return this.getPref('settings.a11y.mouse_keys.enabled').value ?
        this.i18n('deviceOn') :
        this.i18n('deviceOff');
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_MOUSE_KEYS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Ticks for the Mouse Keys accelerations slider. Valid rates are
   * between 0 and 1.
   */
  private mouseKeysAccelerationTicks_(): SliderTick[] {
    return this.buildLinearTicks_({
      min: 0,
      max: 1,
      step: 0.1,
      defaultValue: 0.2,
    });
  }

  /**
   * Ticks for the Mouse Keys max speed slider. Valid rates are
   * between 1 and 10.
   */
  private mouseKeysMaxSpeedTicks_(): SliderTick[] {
    return this.buildLinearTicks_({
      min: 1,
      max: 10,
      step: 1,
      defaultValue: 5,
    });
  }

  /**
   * A helper to build a set of ticks between |min| and |max| (inclusive) spaced
   * evenly by |step|.
   */
  private buildLinearTicks_(data: SliderData): SliderTick[] {
    const ticks: SliderTick[] = [];

    const count = (data.max - data.min) / data.step;
    for (let i = 0; i <= count; i++) {
      const tickValue = data.step * i + data.min;
      ticks.push(this.initTick_({
        tick: tickValue,
        percent: tickValue / data.max,
        defaultValue: data.defaultValue,
      }));
    }
    return ticks;
  }

  /**
   * Initializes i18n labels for ticks arrays.
   */
  private initTick_(data: TickData): SliderTick {
    const value = Math.round(100 * data.percent);
    const strValue = value.toFixed(0);
    const label = data.tick.toFixed(1) === data.defaultValue.toFixed(1) ?
        this.i18n('defaultPercentage', strValue) :
        this.i18n('percentage', strValue);
    return {label: label, value: data.tick, ariaValue: value};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMouseKeysSubpageElement.is]: SettingsMouseKeysSubpageElement;
  }
}

customElements.define(
    SettingsMouseKeysSubpageElement.is, SettingsMouseKeysSubpageElement);
