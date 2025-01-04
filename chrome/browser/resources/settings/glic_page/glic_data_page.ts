// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './glic_data_page.html.js';

export enum SettingsGlicDataPageFeaturePrefName {
  GEOLOCATION_ENABLED = 'glic.geolocation_enabled',
  MICROPHONE_ENABLED = 'glic.microphone_enabled',
  TAB_CONTEXT_ENABLED = 'glic.tab_context_enabled',
}

const SettingsGlicDataPageElementBase = PrefsMixin(PolymerElement);

export interface SettingsGlicDataPageElement {
  $: {
    geolocationToggle: SettingsToggleButtonElement,
    microphoneToggle: SettingsToggleButtonElement,
    tabAccessToggle: SettingsToggleButtonElement,
  };
}

export class SettingsGlicDataPageElement extends
    SettingsGlicDataPageElementBase {
  static get is() {
    return 'settings-glic-data-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-data-page': SettingsGlicDataPageElement;
  }
}

customElements.define(
    SettingsGlicDataPageElement.is, SettingsGlicDataPageElement);
