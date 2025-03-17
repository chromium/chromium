// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './ai_page.html.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName} from './constants.js';

export interface SettingsAiPageElement {
  $: {
    historySearchRow: HTMLElement,
  };
}

// BaseMixin is needed to populate the associatedControl field for search in
// subpages, see crbug.com/378927854.
const SettingsAiPageElementBase = PrefsMixin(BaseMixin(PolymerElement));
export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';
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

      showAutofillAIControl_: {
        type: Boolean,
        value: false,
      },

      showComposeControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showComposeControl'),
      },

      showCompareControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showCompareControl'),
      },

      showHistorySearchControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showHistorySearchControl'),
      },

      showTabOrganizationControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showTabOrganizationControl'),
      },

      showPasswordChangeControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showPasswordChangeControl'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (routes.HISTORY_SEARCH) {
            map.set(routes.HISTORY_SEARCH.path, '#historySearchRowV2');
          }

          if (routes.COMPARE) {
            map.set(routes.COMPARE.path, '#compareRowV2');
          }

          if (routes.OFFER_WRITING_HELP) {
            map.set(routes.OFFER_WRITING_HELP.path, '#composeRowV2');
          }

          if (routes.AI_TAB_ORGANIZATION) {
            map.set(routes.AI_TAB_ORGANIZATION.path, '#tabOrganizationRowV2');
          }

          if (routes.AUTOFILL_AI) {
            map.set(routes.AUTOFILL_AI.path, '#autofillAiRowV2');
          }

          return map;
        },
      },

      historySearchRowSublabel_: {
        type: String,
        value: () => {
          return loadTimeData.getBoolean(
                     'historyEmbeddingsAnswersFeatureEnabled') ?
              loadTimeData.getString('historySearchAnswersSettingSublabel') :
              loadTimeData.getString('historySearchSettingSublabel');
        },
      },
    };
  }

  private enableAiSettingsPageRefresh_: boolean;
  private showAutofillAIControl_: boolean;
  private showComposeControl_: boolean;
  private showCompareControl_: boolean;
  private showHistorySearchControl_: boolean;
  private showTabOrganizationControl_: boolean;
  private showPasswordChangeControl_: boolean;
  private numericUncheckedValues_: FeatureOptInState[];
  private shouldRecordMetrics_: boolean = true;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();
    await this.setShowAutofillAiControl_();
    this.maybeLogVisibilityMetrics_();
  }

  private maybeLogVisibilityMetrics_() {
    // Only record metrics when the user first navigates to the main AI page.
    if (!this.shouldRecordMetrics_ || !this.enableAiSettingsPageRefresh_ ||
        Router.getInstance().getCurrentRoute() !== routes.AI) {
      return;
    }
    this.shouldRecordMetrics_ = false;

    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.AutofillAI',
        this.showAutofillAIControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.HistorySearch',
        this.showHistorySearchControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.Compare', this.showCompareControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.Compose', this.showComposeControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.TabOrganization',
        this.showTabOrganizationControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.PasswordChange',
        this.showPasswordChangeControl_);
  }

  private async setShowAutofillAiControl_() {
    if (loadTimeData.valueExists('showAiSettingsForTesting') &&
        loadTimeData.getBoolean('showAiSettingsForTesting')) {
      this.showAutofillAIControl_ = true;
      return;
    }

    if (!loadTimeData.getBoolean('autofillAiFeatureEnabled')) {
      this.showAutofillAIControl_ = false;
      return;
    }

    // Display the Autofill Ai row only if the user is eligible or if they have
    // data saved.
    // TODO(crbug.com/393318914): Allow the Ai page to be shown if autofill Ai
    // is the only entry on the page.
    this.showAutofillAIControl_ =
        loadTimeData.getBoolean('userEligibleForAutofillAi') ||
        (await EntityDataManagerProxyImpl.getInstance().loadEntityInstances())
                .length > 0;
  }

  private onHistorySearchRowClick_() {
    if (this.enableAiSettingsPageRefresh_) {
      this.recordInteractionMetrics_(
          AiPageInteractions.HISTORY_SEARCH_CLICK,
          'Settings.AiPage.HistorySearchEntryPointClick');
    }

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().HISTORY_SEARCH);
  }

  private onAutofillAiRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.AUTOFILL_AI_CLICK,
        'Settings.AiPage.AutofillAIEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().AUTOFILL_AI);
  }

  private onCompareRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.COMPARE_CLICK,
        'Settings.AiPage.CompareEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().COMPARE);
  }

  private onComposeRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.COMPOSE_CLICK,
        'Settings.AiPage.ComposeEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().OFFER_WRITING_HELP);
  }

  private onTabOrganizationRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.TAB_ORGANIZATION_CLICK,
        'Settings.AiPage.TabOrganizationEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().AI_TAB_ORGANIZATION);
  }

  private onPasswordChangeRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.PASSWORD_CHANGE_CLICK,
        'Settings.AiPage.PasswordChangeEntryPointClick');

    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('passwordChangeSettingsUrl'));
  }

  private recordInteractionMetrics_(
      interaction: AiPageInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageInteractions(interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  private getHistorySearchSublabel_(): string {
    const isAnswersEnabled =
        loadTimeData.getBoolean('historyEmbeddingsAnswersFeatureEnabled');
    if (this.getPref(SettingsAiPageFeaturePrefName.HISTORY_SEARCH).value ===
        FeatureOptInState.ENABLED) {
      return isAnswersEnabled ?
          loadTimeData.getString('historySearchWithAnswersSublabelOn') :
          loadTimeData.getString('historySearchSublabelOn');
    }
    return isAnswersEnabled ?
        loadTimeData.getString('historySearchWithAnswersSublabelOff') :
        loadTimeData.getString('historySearchSublabelOff');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page': SettingsAiPageElement;
  }
}

customElements.define(SettingsAiPageElement.is, SettingsAiPageElement);
