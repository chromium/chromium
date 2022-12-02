// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {getTemplate} from './privacy_sandbox_ad_measurement_subpage.html.js';

export interface SettingsPrivacySandboxAdMeasurementSubpageElement {
  $: {
    adMeasurementToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxAdMeasurementSubpageElementBase =
    PrefsMixin(PolymerElement);

export class SettingsPrivacySandboxAdMeasurementSubpageElement extends
    SettingsPrivacySandboxAdMeasurementSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-ad-measurement-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-ad-measurement-subpage':
        SettingsPrivacySandboxAdMeasurementSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxAdMeasurementSubpageElement.is,
    SettingsPrivacySandboxAdMeasurementSubpageElement);
