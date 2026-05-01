// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './ai_policy_indicator.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageSuggestionsInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {isFeatureDisabledByPolicy} from './ai_policy_indicator.js';
import {getTemplate} from './ai_suggestions_page.html.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState} from './constants.js';

const SettingsAiSuggestionsPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsAiSuggestionsPageElement extends
    SettingsAiSuggestionsPageElementBase {
  static get is() {
    return 'settings-ai-suggestions-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      enterprisePref_: {
        type: Object,
        computed: `computePref(prefs.${
            AiEnterpriseFeaturePrefName.CONTEXTUAL_CUEING})`,
      },
    };
  }

  declare private enterprisePref_: chrome.settingsPrivate.PrefObject;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private recordInteractionMetrics_(
      interaction: AiPageSuggestionsInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageSuggestionsInteractions(interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  private computeNumericUncheckedValues_(): FeatureOptInState[] {
    if (this.isDisabledByPolicy_()) {
      return [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED];
    }
    return [FeatureOptInState.DISABLED];
  }

  private onLearnMoreRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('aiSuggestionsHelpCenterArticleLink'));

    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_LEARN_MORE_CLICKED);
  }

  private onLearnMoreClick_(event: Event) {
    event.stopPropagation();

    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_LEARN_MORE_CLICKED);
  }

  private onSyncSettingsClick_(event: Event) {
    event.stopPropagation();

    this.recordInteractionMetrics_(
        AiPageSuggestionsInteractions.SYNC_SETTINGS_LINK_CLICKED,
        AiPageActions.AI_SUGGESTIONS_SYNC_SETTINGS_CLICKED);
  }

  private onSuggestionsToggleChange_(e: Event) {
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

  private isDisabledByPolicy_(): boolean {
    return isFeatureDisabledByPolicy(this.enterprisePref_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-suggestions-page': SettingsAiSuggestionsPageElement;
  }
}

customElements.define(
    SettingsAiSuggestionsPageElement.is, SettingsAiSuggestionsPageElement);
