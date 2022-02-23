// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of weather units. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TemperatureUnit} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setTemperatureUnit} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

export function inBetween(num: number, minValue: number, maxValue: number) {
  return minValue <= num && num <= maxValue;
}

export class AmbientWeatherUnit extends WithPersonalizationStore {
  static get is() {
    return 'ambient-weather-unit';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Used to refer to the enum values in HTML file.
       */
      temperatureUnit_: {
        type: Object,
        value: TemperatureUnit,
      },

      selectedTemperatureUnit: {
        type: String,
        observer: 'onSelectedTemperatureUnitChanged_',
      }
    };
  }

  private temperatureUnit_: TemperatureUnit;
  private selectedTemperatureUnit: TemperatureUnit;

  private onSelectedTemperatureUnitChanged_(newValue: string, _: string) {
    const num = parseInt(newValue, 10);
    if (!isNaN(num) &&
        inBetween(num, TemperatureUnit.MIN_VALUE, TemperatureUnit.MAX_VALUE)) {
      setTemperatureUnit(
          num as TemperatureUnit, getAmbientProvider(), this.getStore());
    }
  }
}

customElements.define(AmbientWeatherUnit.is, AmbientWeatherUnit);
