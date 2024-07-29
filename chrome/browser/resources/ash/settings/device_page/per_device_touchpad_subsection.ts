// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-touchpad-subsection' allow users to configure their
 * per-device-touchpad subsection settings in system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import './input_device_settings_shared.css.js';
import './per_device_app_installed_row.js';
import './per_device_install_row.js';
import './per_device_subsection_header.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {CompanionAppState, InputDeviceSettingsProviderInterface, SimulateRightClickModifier, Touchpad, TouchpadSettings} from './input_device_settings_types.js';
import {settingsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './per_device_touchpad_subsection.html.js';

const SettingsPerDeviceTouchpadSubsectionElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)));
export class SettingsPerDeviceTouchpadSubsectionElement extends
    SettingsPerDeviceTouchpadSubsectionElementBase {
  static get is() {
    return 'settings-per-device-touchpad-subsection';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      enableTapToClickPref: {
        type: Object,
        value() {
          return {
            key: 'fakeEnableTapToClickPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },

      enableTapDraggingPref: {
        type: Object,
        value() {
          return {
            key: 'fakeEnableTapDraggingPref',
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

      hapticClickSensitivityPref: {
        type: Object,
        value() {
          return {
            key: 'fakeHapticClickSensitivityPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          };
        },
      },

      simulateRightClickPref: {
        type: Object,
        value() {
          return {
            key: 'fakeSimulateRightClickPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: SimulateRightClickModifier.kNone,
          };
        },
      },

      simulateRightClickOptions: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: SimulateRightClickModifier.kNone,
              name: loadTimeData.getString(
                  'touchpadSimulateRightClickOptionDisabled'),
            },
            {
              value: SimulateRightClickModifier.kSearch,
              name: loadTimeData.getString(
                  'touchpadSimulateRightClickOptionSearch'),
            },
            {
              value: SimulateRightClickModifier.kAlt,
              name:
                  loadTimeData.getString('touchpadSimulateRightClickOptionAlt'),
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
      sensitivityValues_: {
        type: Array,
        value: [1, 2, 3, 4, 5],
        readOnly: true,
      },

      /**
       * The click sensitivity values from prefs are [1,3,5] but ChromeVox needs
       * to announce them as [1,2,3].
       */
      hapticClickSensitivityValues_: {
        type: Array,
        value() {
          return [
            {value: 1, ariaValue: 1},
            {value: 3, ariaValue: 2},
            {value: 5, ariaValue: 3},
          ];
        },
        readOnly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      reverseScrollValue: {
        type: Boolean,
        value: false,
      },

      hapticFeedbackValue: {
        type: Boolean,
        value: true,
      },

      touchpad: {type: Object},

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kTouchpadTapToClick,
          Setting.kTouchpadTapDragging,
          Setting.kTouchpadReverseScrolling,
          Setting.kTouchpadAcceleration,
          Setting.kTouchpadScrollAcceleration,
          Setting.kTouchpadSpeed,
          Setting.kTouchpadHapticFeedback,
          Setting.kTouchpadHapticClickSensitivity,
        ]),
      },

      touchpadIndex: {
        type: Number,
      },

      isLastDevice: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isAltClickAndSixPackCustomizationEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableAltClickAndSixPackCustomization');
        },
        readOnly: true,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(enableTapToClickPref.value,' +
          'enableTapDraggingPref.value,' +
          'accelerationPref.value,' +
          'sensitivityPref.value,' +
          'hapticClickSensitivityPref.value,' +
          'simulateRightClickPref.value,' +
          'reverseScrollValue,' +
          'hapticFeedbackValue)',
      'updateSettingsToCurrentPrefs(touchpad)',
    ];
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_TOUCHPAD) {
      return;
    }

    // If multiple touchpads are available, focus on the first one.
    if (this.touchpadIndex === 0) {
      this.attemptDeepLink();
    }
  }

  private touchpad: Touchpad;
  private enableTapToClickPref: chrome.settingsPrivate.PrefObject;
  private enableTapDraggingPref: chrome.settingsPrivate.PrefObject;
  private accelerationPref: chrome.settingsPrivate.PrefObject;
  private sensitivityPref: chrome.settingsPrivate.PrefObject;
  private hapticClickSensitivityPref: chrome.settingsPrivate.PrefObject;
  private simulateRightClickPref: chrome.settingsPrivate.PrefObject;
  private reverseScrollValue: boolean;
  private hapticFeedbackValue: boolean;
  private isInitialized: boolean = false;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private touchpadIndex: number;
  private isLastDevice: boolean;
  isAltClickAndSixPackCustomizationEnabled: boolean;
  private isRevampWayfindingEnabled_: boolean;

  private showInstallAppRow(): boolean {
    return this.touchpad.appInfo?.state === CompanionAppState.kAvailable;
  }

  private updateSettingsToCurrentPrefs(): void {
    // `updateSettingsToCurrentPrefs` gets called when the `keyboard` object
    // gets updated. This subsection element can be reused multiple times so we
    // need to reset `isInitialized` so we do not make unneeded API calls.
    this.isInitialized = false;
    this.set(
        'enableTapToClickPref.value', this.touchpad.settings.tapToClickEnabled);
    this.set(
        'simulateRightClickPref.value',
        this.touchpad.settings.simulateRightClick);
    this.set(
        'enableTapDraggingPref.value',
        this.touchpad.settings.tapDraggingEnabled);
    this.set(
        'accelerationPref.value', this.touchpad.settings.accelerationEnabled);
    this.set('sensitivityPref.value', this.touchpad.settings.sensitivity);
    this.set(
        'hapticClickSensitivityPref.value',
        this.touchpad.settings.hapticSensitivity);
    this.reverseScrollValue = this.touchpad.settings.reverseScrolling;
    this.hapticFeedbackValue = this.touchpad.settings.hapticEnabled;
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

  private onTouchpadReverseScrollRowClicked_(): void {
    this.reverseScrollValue = !this.reverseScrollValue;
  }

  private onTouchpadHapticFeedbackRowClicked_(): void {
    this.hapticFeedbackValue = !this.hapticFeedbackValue;
  }

  private onSettingsChanged(): void {
    if (!this.isInitialized) {
      return;
    }

    const newSettings: TouchpadSettings = {
      ...this.touchpad.settings,
      tapToClickEnabled: this.enableTapToClickPref.value,
      tapDraggingEnabled: this.enableTapDraggingPref.value,
      accelerationEnabled: this.accelerationPref.value,
      sensitivity: this.sensitivityPref.value,
      hapticSensitivity: this.hapticClickSensitivityPref.value,
      simulateRightClick: this.simulateRightClickPref.value,
      reverseScrolling: this.reverseScrollValue,
      hapticEnabled: this.hapticFeedbackValue,
    };

    if (settingsAreEqual(newSettings, this.touchpad.settings)) {
      return;
    }

    this.touchpad.settings = newSettings;
    this.inputDeviceSettingsProvider.setTouchpadSettings(
        this.touchpad.id, this.touchpad.settings);
  }

  private getTouchpadName(): string {
    return this.touchpad.isExternal ? this.touchpad.name :
                                      this.i18n('builtInTouchpadName');
  }

  private getLabelWithoutLearnMore(stringName: string): string|TrustedHTML {
    const tempEl = document.createElement('div');
    const localizedString = this.i18nAdvanced(stringName);
    tempEl.innerHTML = localizedString;

    const nodesToDelete: Node[] = [];
    tempEl.childNodes.forEach((node) => {
      // Remove elements with the <a> tag
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        nodesToDelete.push(node);
        return;
      }
    });

    nodesToDelete.forEach((node) => {
      tempEl.removeChild(node);
    });

    return tempEl.innerHTML;
  }

  private getTouchpadAccelerationDescription(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('touchpadAccelerationDescription');
    }
    return '';
  }

  private getTouchpadTapDraggingDescription(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('tapDraggingDescription');
    }
    return '';
  }

  private getTouchpadTapToClickDescription(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('touchpadTapToClickDescription');
    }
    return '';
  }

  private isCompanionAppInstalled(): boolean {
    return this.touchpad.appInfo?.state === CompanionAppState.kInstalled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-touchpad-subsection':
        SettingsPerDeviceTouchpadSubsectionElement;
  }
}

customElements.define(
    SettingsPerDeviceTouchpadSubsectionElement.is,
    SettingsPerDeviceTouchpadSubsectionElement);
