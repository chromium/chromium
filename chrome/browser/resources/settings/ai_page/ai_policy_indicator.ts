// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-policy-indicator' is an indicator that informs the user if the
 * feature is controlled by policy.
 */
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PrefKeyObserverMixinLit} from '../controls/pref_key_observer_mixin_lit.js';

import {getCss} from './ai_policy_indicator.css.js';
import {getHtml} from './ai_policy_indicator.html.js';
import {AiEnterpriseFeaturePrefName, ChromeSuggestionsSettingsValue, ModelExecutionEnterprisePolicyValue} from './constants.js';

export function isFeatureDisabledByPolicy(
    enterprisePref: chrome.settingsPrivate.PrefObject|undefined): boolean {
  if (!enterprisePref) {
    return false;
  }
  if (enterprisePref.key === AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING) {
    return enterprisePref.value === ChromeSuggestionsSettingsValue.DISABLED;
  }
  return enterprisePref.value === ModelExecutionEnterprisePolicyValue.DISABLE;
}

const SettingsAiPolicyIndicatorElementBase =
    PrefKeyObserverMixinLit(CrLitElement);

export class SettingsAiPolicyIndicatorElement extends
    SettingsAiPolicyIndicatorElementBase {
  static get is() {
    return 'settings-ai-policy-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pref: {type: Object},
    };
  }

  protected accessor pref: chrome.settingsPrivate.PrefObject|undefined =
      undefined;

  protected isFeatureDisabledByPolicy_(): boolean {
    return isFeatureDisabledByPolicy(this.pref);
  }
}

export type AiPolicyIndicatorElement = SettingsAiPolicyIndicatorElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-policy-indicator': SettingsAiPolicyIndicatorElement;
  }
}

customElements.define(
    SettingsAiPolicyIndicatorElement.is, SettingsAiPolicyIndicatorElement);
