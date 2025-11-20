// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../settings_page/settings_subpage.js';
import './security_page_feature_row.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import {loadTimeData} from '../../i18n_setup.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import {SafeBrowsingSetting} from '../safe_browsing_types.js';

import type {SecurityPageFeatureRowElement} from './security_page_feature_row.js';
import {getTemplate} from './security_page_v2.html.js';

/** Enumeration of all security settings bundle modes.*/
// LINT.IfChange(SecuritySettingsBundleSetting)
export enum SecuritySettingsBundleSetting {
  STANDARD = 0,
  ENHANCED = 1,
}
// LINT.ThenChange(/components/safe_browsing/core/common/safe_browsing_prefs.h:SecuritySettingsBundleSetting)

export interface SettingsSecurityPageV2Element {
  $: {
    bundlesRadioGroup: SettingsRadioGroupElement,
    resetBundleToDefaultsButton: CrButtonElement,
    securitySettingsBundleEnhanced: ControlledRadioButtonElement,
    securitySettingsBundleStandard: ControlledRadioButtonElement,
    safeBrowsingRadioGroup: SettingsRadioGroupElement,
    safeBrowsingRow: SecurityPageFeatureRowElement,
  };
}

const SettingsSecurityPageV2ElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

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
      securitySettingsBundleSettingEnum_: {
        type: Object,
        value: SecuritySettingsBundleSetting,
      },

      safeBrowsingSettingEnum_: {
        type: Object,
        value: SafeBrowsingSetting,
      },

      isResetToDefaultsButtonHidden_: {
        type: Boolean,
        computed: 'computeIsResetToDefaultsButtonHidden_(' +
            'isResettingToDefaults_,' +
            'prefs.generated.security_settings_bundle.value,' +
            'prefs.generated.safe_browsing.*),',
      },

      // Whether the security-setting-bundle is being reset to default.
      isResettingToDefaults_: {
        type: Boolean,
        value: false,
      },

      safeBrowsingOff_: {
        type: Array,
        value: () => [SafeBrowsingSetting.DISABLED],
      },

      safeBrowsingStateTextMap_: {
        type: Object,
        value: () => ({
          [SafeBrowsingSetting.ENHANCED]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [SafeBrowsingSetting.STANDARD]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
          [SafeBrowsingSetting.DISABLED]:
              loadTimeData.getString('securityFeatureRowStateOff'),
        }),
      },
    };
  }

  declare private isResettingToDefaults_: boolean;
  declare private isResetToDefaultsButtonHidden_: boolean;
  declare private safeBrowsingOff_: SafeBrowsingSetting[];
  declare private safeBrowsingStateTextMap_: Object;

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  private getBundleSetting_() {
    return this.getPref('generated.security_settings_bundle').value;
  }

  private getDefaultSafeBrowsingValue_(
      bundleSetting: SecuritySettingsBundleSetting) {
    return loadTimeData.getInteger(
        (bundleSetting === SecuritySettingsBundleSetting.ENHANCED) ?
            'securityEnhancedBundleSafeBrowsingDefault' :
            'securityStandardBundleSafeBrowsingDefault');
  }

  private computeIsResetToDefaultsButtonHidden_() {
    if (this.isResettingToDefaults_) {
      return true;
    }

    const bundleSetting = this.getBundleSetting_();

    const prefsToCheck = [{
      prefKey: 'generated.safe_browsing',
      defaultValue: this.getDefaultSafeBrowsingValue_(bundleSetting),
    }];
    for (const prefToCheck of prefsToCheck) {
      const pref = this.getPref(prefToCheck.prefKey);
      if (pref.value !== prefToCheck.defaultValue &&
          pref.controlledBy == null) {
        return false;
      }
    }

    return true;
  }

  private onSecurityBundleChanged_() {
    this.resetBundleToDefaults_();
  }

  private onResetBundleToDefaultsButtonClick_() {
    this.resetBundleToDefaults_();
  }

  private resetBundleToDefaults_() {
    this.isResettingToDefaults_ = true;
    const bundleSetting = this.getBundleSetting_();
    this.setPrefValue(
        'generated.safe_browsing',
        this.getDefaultSafeBrowsingValue_(bundleSetting));
    this.isResettingToDefaults_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);
