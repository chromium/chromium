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

      selectedTemperatureUnit_: {
        type: TemperatureUnit,
        value: null,
        observer: 'onSelectedTemperatureUnitChanged_',
      }
    };
  }

  private temperatureUnit_: TemperatureUnit;
  private selectedTemperatureUnit_: TemperatureUnit|null;

  private onSelectedTemperatureUnitChanged_(
      newValue: TemperatureUnit, oldValue: TemperatureUnit) {
    console.log(oldValue, newValue);
    // TODO: implementation will be updated in followup CLs.
  }
}

customElements.define(AmbientWeatherUnit.is, AmbientWeatherUnit);
