// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-pointing-stick-subsection' allow users to configure their
 * per-device-pointing-stick subsection settings in system settings.
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

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, PointingStick} from './input_device_settings_types.js';
import {getTemplate} from './per_device_pointing_stick_subsection.html.js';

export class SettingsPerDevicePointingStickSubsectionElement extends
    PolymerElement {
  static get is(): string {
    return 'settings-per-device-pointing-stick-subsection';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      primaryRightPref: {
        type: Object,
        value() {
          return {
            key: 'fakePrimaryRightPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      accelerationPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAccelerationPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },

      sensitivityPref: {
        type: Object,
        value() {
          return {
            key: 'fakeSensitivityPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          };
        },
      },

      swapPrimaryOptions: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: false,
              name: loadTimeData.getString('primaryMouseButtonLeft'),
            },
            {
              value: true,
              name: loadTimeData.getString('primaryMouseButtonRight'),
            },
          ];
        },
      },

      /**
       * TODO(michaelpg): settings-slider should optionally take a min and max
       * so we don't have to generate a simple range of natural numbers
       * ourselves. These values match the TouchpadSensitivity enum in
       * enums.xml.
       */
      sensitivityValues: {
        type: Array,
        value: [1, 2, 3, 4, 5],
        readOnly: true,
      },

      pointingStick: {type: Object},
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(primaryRightPref.value,' +
          'accelerationPref.value,' +
          'sensitivityPref.value)',
      'updateSettingsToCurrentPrefs(pointingStick)',
    ];
  }

  private pointingStick: PointingStick;
  private sensitivityValues: number[];
  private swapPrimaryOptions: number[];
  private primaryRightPref: chrome.settingsPrivate.PrefObject;
  private accelerationPref: chrome.settingsPrivate.PrefObject;
  private sensitivityPref: chrome.settingsPrivate.PrefObject;
  private isInitialized: boolean = false;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

  private updateSettingsToCurrentPrefs(): void {
    this.set('primaryRightPref.value', this.pointingStick.settings.swapRight);
    this.set(
        'accelerationPref.value',
        this.pointingStick.settings.accelerationEnabled);
    this.set('sensitivityPref.value', this.pointingStick.settings.sensitivity);
    this.isInitialized = true;
  }

  private onLearnMoreLinkClicked_(event: Event): void {
    const path = event.composedPath();
    if (!Array.isArray(path) || !path.length) {
      return;
    }

    if ((path[0] as HTMLElement).tagName === 'A') {
      // Do not toggle reverse scrolling if the contained link is clicked.
      event.stopPropagation();
    }
  }

  private onSettingsChanged(): void {
    // TODO(wangdanny): Implement onSettingsChanged.
    if (!this.isInitialized) {
      return;
    }
    this.pointingStick.settings = {
      ...this.pointingStick.settings,
      swapRight: this.primaryRightPref.value,
      accelerationEnabled: this.accelerationPref.value,
      sensitivity: this.sensitivityPref.value,
    };
    this.inputDeviceSettingsProvider.setPointingStickSettings(
        this.pointingStick.id, this.pointingStick.settings);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-pointing-stick-subsection':
        SettingsPerDevicePointingStickSubsectionElement;
  }
}

customElements.define(
    SettingsPerDevicePointingStickSubsectionElement.is,
    SettingsPerDevicePointingStickSubsectionElement);
