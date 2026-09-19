// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './ai_policy_indicator.js';
import '../settings_page/settings_subpage.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageSuggestionsInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {isFeatureDisabledByPolicy} from './ai_policy_indicator.js';
import {getCss} from './ai_suggestions_page.css.js';
import {getHtml} from './ai_suggestions_page.html.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState} from './constants.js';

const SettingsAiSuggestionsPageElementBase =
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export class SettingsAiSuggestionsPageElement extends
    SettingsAiSuggestionsPageElementBase {
  static get is() {
    return 'settings-ai-suggestions-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enterprisePref_: {type: Object},
    };
  }

  protected accessor enterprisePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(
        AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING, 'enterprisePref_');
  }

  private recordInteractionMetrics_(
      interaction: AiPageSuggestionsInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageSuggestionsInteractions(interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  protected computeNumericUncheckedValues_(): FeatureOptInState[] {
    if (this.isDisabledByPolicy_()) {
      return [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED];
    }
    return [FeatureOptInState.DISABLED];
  }

  protected onLearnMoreLinkClick_(event: Event) {
    event.stopPropagation();

    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_LEARN_MORE_CLICKED);
  }

  protected onSyncSettingsLinkClick_(event: Event) {
    event.stopPropagation();

    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.SYNC_SETTINGS_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_SYNC_SETTINGS_CLICKED);
  }

  protected onShowSuggestionsToggleSettingsBooleanControlChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    if (toggle.checked) {
      this.recordInteractionMetrics_(
          AiPageSuggestionsInteractions.SUGGESTIONS_ENABLED,
          AiPageActions.AI_SUGGESTIONS_ENABLED);
      return;
    }
    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.SUGGESTIONS_DISABLED,
        AiPageActions.AI_SUGGESTIONS_DISABLED);
  }

  protected isDisabledByPolicy_(): boolean {
    return isFeatureDisabledByPolicy(this.enterprisePref_);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

export type AiSuggestionsPageElement = SettingsAiSuggestionsPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-suggestions-page': SettingsAiSuggestionsPageElement;
  }
}

customElements.define(
    SettingsAiSuggestionsPageElement.is, SettingsAiSuggestionsPageElement);
