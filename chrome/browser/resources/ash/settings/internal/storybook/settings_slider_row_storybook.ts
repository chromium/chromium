// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/v2/settings_slider_row.js';

import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_slider_row_storybook.html.js';

export class SettingsSliderRowStorybook extends PolymerElement {
  static get is() {
    return 'settings-slider-row-storybook' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sliderValueWithScale_: {
        type: Number,
        value: 0.5,
      },

      virtualManagedPref_: {
        type: Object,
        value: {
          key: 'virtual_managed_pref',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 5,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        },
      },

      ticks_: {
        type: Array,
        value: () => {
          return [
            {label: '0', value: 0},
            {label: '5', value: 5},
            {label: '10', value: 10},
            {label: '15', value: 15},
            {label: '20', value: 20},
          ];
        },
      },

      hideLabel_: {
        type: Boolean,
        value: false,
      },

      updateValueInstantly_: {
        type: Boolean,
        value: false,
      },

      disabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private sliderValueWithScale_: number;
  private virtualManagedPref_: chrome.settingsPrivate.PrefObject<number>;
  private ticks_: SliderTick[];
  private hideLabel_: boolean;
  private updateValueInstantly_: boolean;
  private disabled_: boolean;

  private onScaleSliderChange_(event: CustomEvent<number>): void {
    this.sliderValueWithScale_ = event.detail;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSliderRowStorybook.is]: SettingsSliderRowStorybook;
  }
}

customElements.define(
    SettingsSliderRowStorybook.is, SettingsSliderRowStorybook);
