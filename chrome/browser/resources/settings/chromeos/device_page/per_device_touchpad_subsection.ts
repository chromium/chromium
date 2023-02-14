// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-touchpad-subsection' allow users to configure their
 * per-device-touchpad subsection settings in system settings.
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

import {Touchpad} from './input_device_settings_types.js';
import {getTemplate} from './per_device_touchpad_subsection.html.js';

export class SettingsPerDeviceTouchpadSubsectionElement extends PolymerElement {
  static get is(): string {
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

      scrollAccelerationPref: {
        type: Object,
        value() {
          return {
            key: 'fakeScrollAccelerationPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },

      scrollSensitivityPref: {
        type: Object,
        value() {
          return {
            key: 'fakeScrollSensitivityPref',
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

      /**
       * TODO(khorimoto): Remove this conditional once the feature is launched.
       */
      allowScrollSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowScrollSettings');
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

      reverseScrollValue: {
        type: Boolean,
        value: false,
      },

      hapticFeedbackValue: {
        type: Boolean,
        value: true,
      },

      touchpad: {type: Object},
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(enableTapToClickPref.value,' +
          'enableTapDraggingPref.value,' +
          'accelerationPref.value,' +
          'sensitivityPref.value,' +
          'scrollAccelerationPref.value,' +
          'scrollSensitivityPref.value,' +
          'hapticClickSensitivityPref.value,' +
          'reverseScrollValue)',
      'updateSettingsToCurrentPrefs(touchpad)',
    ];
  }

  private touchpad: Touchpad;
  private enableTapToClickPref: chrome.settingsPrivate.PrefObject;
  private enableTapDraggingPref: chrome.settingsPrivate.PrefObject;
  private accelerationPref: chrome.settingsPrivate.PrefObject;
  private sensitivityPref: chrome.settingsPrivate.PrefObject;
  private scrollAccelerationPref: chrome.settingsPrivate.PrefObject;
  private scrollSensitivityPref: chrome.settingsPrivate.PrefObject;
  private hapticClickSensitivityPref: chrome.settingsPrivate.PrefObject;
  private reverseScrollValue: boolean;
  private hapticFeedbackValue: boolean;
  private isInitialized: boolean = false;

  private updateSettingsToCurrentPrefs(): void {
    this.set(
        'enableTapToClickPref.value', this.touchpad.settings.tapToClickEnabled);
    this.set(
        'enableTapDraggingPref.value',
        this.touchpad.settings.tapDraggingEnabled);
    this.set(
        'accelerationPref.value', this.touchpad.settings.accelerationEnabled);
    this.set('sensitivityPref.value', this.touchpad.settings.sensitivity);
    this.set(
        'scrollAccelerationPref.value',
        this.touchpad.settings.scrollAcceleration);
    this.set(
        'scrollSensitivityPref.value',
        this.touchpad.settings.scrollSensitivity);
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
    // TODO(wangdanny): Implement onSettingsChanged.
    if (!this.isInitialized) {
      return;
    }
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
