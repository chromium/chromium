// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';

import type {SettingsCollapseRadioButtonElement} from './collapse_radio_button.js';
import {getTemplate} from './security_page_v2.html.js';

/** Enumeration of all security settings bundle modes.*/
// LINT.IfChange(SecuritySettingsBundleSetting)
export enum SecuritySettingsBundleSetting {
  STANDARD = 0,
  ENHANCED = 1,
}
// LINT.ThenChange(/chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h:SecuritySettingsBundleSetting)
export interface SettingsSecurityPageV2Element {
  $: {
    securitySettingsBundleEnhanced: SettingsCollapseRadioButtonElement,
    bundlesRadioGroup: SettingsRadioGroupElement,
    securitySettingsBundleStandard: SettingsCollapseRadioButtonElement,
  };
}

const SettingsSecurityPageV2ElementBase = PrefsMixin(PolymerElement);

export class SettingsSecurityPageV2Element extends
    SettingsSecurityPageV2ElementBase {
  static get is() {
    return 'settings-security-page-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Valid security settings bundle states.*/
      securitySettingsBundleSettingEnum_: {
        type: Object,
        value: SecuritySettingsBundleSetting,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);
