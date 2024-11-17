// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageHistorySearchInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {getAiLearnMoreUrl} from './ai_learn_more_url_util.js';
import {AiEnterpriseFeaturePrefName, AiPageActions, FeatureOptInState} from './constants.js';
import {getTemplate} from './history_search_page.html.js';

const SettingsHistorySearchPageElementBase = PrefsMixin(PolymerElement);
export class SettingsHistorySearchPageElement extends
    SettingsHistorySearchPageElementBase {
  static get is() {
    return 'settings-history-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableAiSettingsPageRefresh_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableAiSettingsPageRefresh'),
      },

      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      isAnswersFeatureEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('historyEmbeddingsAnswersFeatureEnabled'),
      },

      numericUncheckedValues_: {
        type: Array,
        value: () =>
            [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED],
      },

      toggleSubLabel_: {
        type: String,
        value: () => {
          return loadTimeData.getBoolean(
                     'historyEmbeddingsAnswersFeatureEnabled') ?
              loadTimeData.getString('historySearchAnswersSettingSublabel') :
              loadTimeData.getString('historySearchSettingSublabel');
        },
      },

      enterprisePref_: {
        type: Object,
        computed:
            `computePref(prefs.${AiEnterpriseFeaturePrefName.HISTORY_SEARCH})`,
      },
    };
  }

  private enableAiSettingsPageRefresh_: boolean;
  private numericUncheckedValues_: FeatureOptInState[];
  private enterprisePref_: chrome.settingsPrivate.PrefObject;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private recordInteractionMetrics_(
      interaction: AiPageHistorySearchInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageHistorySearchInteractions(
        interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  private onHistorySearchLinkoutClick_() {
    this.recordInteractionMetrics_(
        AiPageHistorySearchInteractions.FEATURE_LINK_CLICKED,
        AiPageActions.HISTORY_SEARCH_FEATURE_LINK_CLICKED);

    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('historySearchDataHomeUrl'));
  }

  private onLearnMoreClick_(event: Event) {
    // Stop the propagation of events, so that clicking on the 'Learn More' link
    // won't trigger the external linkout action on the parent cr-link-row
    // element.
    event.stopPropagation();
    this.recordInteractionMetrics_(
        AiPageHistorySearchInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.HISTORY_SEARCH_LEARN_MORE_CLICKED);
  }

  private onHistorySearchToggleChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    if (toggle.checked) {
      this.recordInteractionMetrics_(
          AiPageHistorySearchInteractions.HISTORY_SEARCH_ENABLED,
          AiPageActions.HISTORY_SEARCH_ENABLED);
      return;
    }
    this.recordInteractionMetrics_(
        AiPageHistorySearchInteractions.HISTORY_SEARCH_DISABLED,
        AiPageActions.HISTORY_SEARCH_DISABLED);
  }

  private getLearnMoreUrl_(): string {
    return getAiLearnMoreUrl(
        this.enterprisePref_,
        loadTimeData.getString('historySearchLearnMoreUrl'),
        loadTimeData.getString('historySearchLearnMoreManagedUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-history-search-page': SettingsHistorySearchPageElement;
  }
}

customElements.define(
    SettingsHistorySearchPageElement.is, SettingsHistorySearchPageElement);
