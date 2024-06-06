// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_slider_v2.js';
import './settings_row.js';

import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseRowMixin} from './base_row_mixin.js';
import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {SettingsRowElement} from './settings_row.js';
import {getTemplate} from './settings_slider_row.html.js';
import {SettingsSliderV2Element} from './settings_slider_v2.js';

export interface SettingsSliderRowElement {
  $: {
    slider: SettingsSliderV2Element,
    internalRow: SettingsRowElement,
  };
}

const SettingsSliderRowElementBase =
    PrefControlMixinInternal(BaseRowMixin(PolymerElement));

export class SettingsSliderRowElement extends SettingsSliderRowElementBase {
  static get is() {
    return 'settings-slider-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ...SettingsSliderV2Element.sharedProperties,

      updateValueInstantly: {
        type: Boolean,
        value: false,
      },
    };
  }

  ticks: SliderTick[]|number[];
  scale: number;
  min: number;
  max: number;
  hideLabel: boolean;
  minLabel: string;
  maxLabel: string;
  hideMarkers: boolean;
  updateValueInstantly: boolean;
  value: number;

  override focus(): void {
    this.$.slider.focus();
  }

  private propagateChangeEvent_({detail}: CustomEvent<number>): void {
    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSliderRowElement.is]: SettingsSliderRowElement;
  }
}

customElements.define(SettingsSliderRowElement.is, SettingsSliderRowElement);
