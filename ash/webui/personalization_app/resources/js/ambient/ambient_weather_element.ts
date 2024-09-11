// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of weather units. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../geolocation_dialog.js';

import {TemperatureUnit} from '../../personalization_app.mojom-webui.js';
import {isCrosPrivacyHubLocationEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {inBetween} from '../utils.js';

import {enableGeolocationForSystemServices, initializeData, setTemperatureUnit} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';
import {getTemplate} from './ambient_weather_element.html.js';

export class AmbientWeatherUnitElement extends WithPersonalizationStore {
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

      geolocationPermissionEnabled_: {
        type: Boolean,
        value: null,
      },

      geolocationIsUserModifiable_: {
        type: Boolean,
        value: null,
      },

      shouldShowGeolocationWarningText_: {
        type: Boolean,
        computed: 'computeShouldShowGeolocationWarningText_(' +
            'geolocationPermissionEnabled_),',
        value: false,
      },
    };
  }

  private temperatureUnit_: TemperatureUnit;
  private selectedTemperatureUnit: string;
  private geolocationPermissionEnabled_: boolean|null;
  private geolocationIsUserModifiable_: boolean|null;
  private shouldShowGeolocationDialog_: boolean;
  private shouldShowGeolocationWarningText_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    AmbientObserver.initAmbientObserverIfNeeded();

    this.watch<AmbientWeatherUnitElement['geolocationPermissionEnabled_']>(
        'geolocationPermissionEnabled_',
        state => state.ambient.geolocationPermissionEnabled);
    this.watch<AmbientWeatherUnitElement['geolocationIsUserModifiable_']>(
        'geolocationIsUserModifiable_',
        state => state.ambient.geolocationIsUserModifiable);
    this.updateFromStore();

    initializeData(getAmbientProvider(), this.getStore());
  }

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

  private computeShouldShowGeolocationWarningText_(): boolean {
    // Warning text should be guarded with the Privacy Hub feature flag.
    return isCrosPrivacyHubLocationEnabled() &&
        this.geolocationPermissionEnabled_ === false;
  }

  private openGeolocationDialog_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#".
    e.detail.event.preventDefault();
    e.stopPropagation();

    // Geolocation Dialog only exists in the Privacy Hub context.
    if (!isCrosPrivacyHubLocationEnabled()) {
      console.error(
          'Geolocation Dialog triggered when the Privacy Hub flag is disabled');
      return;
    }

    // Show the dialog to let users enable system location inline.
    this.shouldShowGeolocationDialog_ = true;
  }

  private onGeolocationDialogClose_(): void {
    this.shouldShowGeolocationDialog_ = false;
  }

  // Callback for user clicking 'Allow' on the geolocation dialog.
  private onGeolocationEnabled_(): void {
    // Enable system geolocation permission for all system services.
    enableGeolocationForSystemServices(this.getStore());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ambient-weather-unit': AmbientWeatherUnitElement;
  }
}

customElements.define(AmbientWeatherUnitElement.is, AmbientWeatherUnitElement);
