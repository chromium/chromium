// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of weather units. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import '../../common/styles.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {TemperatureUnit} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setTemperatureUnit} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './ambient_weather_element.html.js';

export function inBetween(num: number, minValue: number, maxValue: number) {
  return minValue <= num && num <= maxValue;
}

export class AmbientWeatherUnit extends WithPersonalizationStore {
  static get is() {
    return 'ambient-weather-unit';
  }

  static get template() {
    return getTemplate();
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
      },
    };
  }

  private temperatureUnit_: TemperatureUnit;
  private selectedTemperatureUnit: string;

  private onSelectedTemperatureUnitChanged_(value: string) {
    const num = parseInt(value, 10);
    if (isNaN(num) ||
        !inBetween(num, TemperatureUnit.MIN_VALUE, TemperatureUnit.MAX_VALUE)) {
      console.warn('Unexpected temperature unit received', value);
      return;
    }
    setTemperatureUnit(
        num as TemperatureUnit, getAmbientProvider(), this.getStore());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ambient-weather-unit': AmbientWeatherUnit;
  }
}

customElements.define(AmbientWeatherUnit.is, AmbientWeatherUnit);
