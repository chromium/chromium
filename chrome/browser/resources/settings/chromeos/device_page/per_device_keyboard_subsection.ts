// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'per-device-keyboard-subsection' allow users to configure their
 * per-device-keyboard subsection settings in system settings.
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

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './per_device_keyboard_subsection.html.js';

export class SettingsPerDeviceKeyboardSubsectionElement extends PolymerElement {
  static get is(): string {
    return 'settings-per-device-keyboard-subsection';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      topRowAreFunctionKeysPref: {
        type: Object,
        value() {
          return {
            key: 'fakeTopRowAreFunctionKeysPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      blockMetaFunctionKeyRewritesPref: {
        type: Object,
        value() {
          return {
            key: 'fakeBlockMetaFunctionKeyRewritesPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      enableAutoRepeatPref: {
        type: Object,
        value() {
          return {
            key: 'fakeEnableAutoRepeatPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },

      autoRepeatDelaysPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAutoRepeatDelaysPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 500,
          };
        },
      },

      autoRepeatIntervalsPref: {
        type: Object,
        value() {
          return {
            key: 'fakeAutoRepeatIntervalsPref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 50,
          };
        },
      },

      /**
       * Auto-repeat delays (in ms) for the corresponding slider values, from
       * long to short. The values were chosen to provide a large range while
       * giving several options near the defaults.
       */
      autoRepeatDelays: {
        type: Array,
        value: [2000, 1500, 1000, 500, 300, 200, 150],
        readOnly: true,
      },

      /**
       * Auto-repeat intervals (in ms) for the corresponding slider values, from
       * long to short. The slider itself is labeled "rate", the inverse of
       * interval, and goes from slow (long interval) to fast (short interval).
       */
      autoRepeatIntervals: {
        type: Array,
        value: [2000, 1000, 500, 300, 200, 100, 50, 30, 20],
        readOnly: true,
      },

      keyboard: {
        type: Object,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(topRowAreFunctionKeysPref.value,' +
          'blockMetaFunctionKeyRewritesPref.value,' +
          'enableAutoRepeatPref.value,' +
          'autoRepeatDelaysPref.value,' +
          'autoRepeatIntervalsPref.value)',
    ];
  }

  private autoRepeatDelays: number[];
  private autoRepeatIntervals: number[];
  private keyboard: Object;
  private topRowAreFunctionKeysPref: chrome.settingsPrivate.PrefObject;
  private blockMetaFunctionKeyRewritesPref: chrome.settingsPrivate.PrefObject;
  private enableAutoRepeatPref: chrome.settingsPrivate.PrefObject;
  private autoRepeatDelaysPref: chrome.settingsPrivate.PrefObject;
  private autoRepeatIntervalsPref: chrome.settingsPrivate.PrefObject;

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
  }

  private onRemapKeyboardKeysTap(): void {
    // TODO(yyhyyh@): Create keyboard remapping page and its route.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-per-device-keyboard-subsection':
        SettingsPerDeviceKeyboardSubsectionElement;
  }
}

customElements.define(
    SettingsPerDeviceKeyboardSubsectionElement.is,
    SettingsPerDeviceKeyboardSubsectionElement);
