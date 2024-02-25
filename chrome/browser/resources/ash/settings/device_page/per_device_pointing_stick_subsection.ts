// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-pointing-stick-subsection' allow users to configure their
 * per-device-pointing-stick subsection settings in system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import './input_device_settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, PointingStick, PointingStickSettings} from './input_device_settings_types.js';
import {settingsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_pointing_stick_subsection.html.js';

const SettingsPerDevicePointingStickSubsectionElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)));
export class SettingsPerDevicePointingStickSubsectionElement extends
    SettingsPerDevicePointingStickSubsectionElementBase {
  static get is() {
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

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kPointingStickAcceleration,
          Setting.kPointingStickSpeed,
          Setting.kPointingStickSwapPrimaryButtons,
        ]),
      },

      pointingStickIndex: {
        type: Number,
      },

      isLastDevice: {
        type: Boolean,
        reflectToAttribute: true,
      },
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

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_POINTING_STICK) {
      return;
    }

    // If multiple pointing sticks are available, focus on the first one.
    if (this.pointingStickIndex === 0) {
      this.attemptDeepLink();
    }
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
  private pointingStickIndex: number;
  private isLastDevice: boolean;

  private updateSettingsToCurrentPrefs(): void {
    // `updateSettingsToCurrentPrefs` gets called when the `keyboard` object
    // gets updated. This subsection element can be reused multiple times so we
    // need to reset `isInitialized` so we do not make unneeded API calls.
    this.isInitialized = false;
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
    if (!this.isInitialized) {
      return;
    }

    const newSettings: PointingStickSettings = {
      ...this.pointingStick.settings,
      swapRight: this.primaryRightPref.value,
      accelerationEnabled: this.accelerationPref.value,
      sensitivity: this.sensitivityPref.value,
    };

    if (settingsAreEqual(newSettings, this.pointingStick.settings)) {
      return;
    }

    this.pointingStick.settings = newSettings;
    this.inputDeviceSettingsProvider.setPointingStickSettings(
        this.pointingStick.id, this.pointingStick.settings);
  }

  private getPointingStickName(): string {
    return this.pointingStick.isExternal ?
        this.pointingStick.name :
        this.i18n('builtInPointingStickName');
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
