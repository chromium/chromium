// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-policy-indicator' is an indicator that informs the user if the
 * feature is controlled by policy.
 */
import '../settings_shared.css.js';

import {PrefControlMixin} from '/shared/settings/controls/pref_control_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_policy_indicator.html.js';
import {ModelExecutionEnterprisePolicyValue} from './constants.js';

export function isFeatureDisabledByPolicy(
    enterprisePref: chrome.settingsPrivate.PrefObject|undefined): boolean {
  return !!enterprisePref &&
      enterprisePref.value === ModelExecutionEnterprisePolicyValue.DISABLE;
}

const SettingsAiPolicyIndicatorBase = PrefControlMixin(PolymerElement);

export class SettingsAiPolicyIndicator extends SettingsAiPolicyIndicatorBase {
  static get is() {
    return 'settings-ai-policy-indicator';
  }

  static get template() {
    return getTemplate();
  }

  private isFeatureDisabledByPolicy_(): boolean {
    return isFeatureDisabledByPolicy(this.pref);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-policy-indicator': SettingsAiPolicyIndicator;
  }
}

customElements.define(SettingsAiPolicyIndicator.is, SettingsAiPolicyIndicator);
