// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_section.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './ai_page.html.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName} from './constants.js';

const SettingsAiPageElementBase = SettingsViewMixin(PrefsMixin(PolymerElement));
export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
    };
  }

  declare private showComposeControl_: boolean;
  declare private showCompareControl_: boolean;
  declare private showHistorySearchControl_: boolean;
  declare private showTabOrganizationControl_: boolean;
  declare private showPasswordChangeControl_: boolean;

  private shouldRecordMetrics_: boolean = true;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.maybeLogVisibilityMetrics_();
  }

  private maybeLogVisibilityMetrics_() {
    // Only record metrics when the user first navigates to the main AI page.
    if (!this.shouldRecordMetrics_ ||
        Router.getInstance().getCurrentRoute() !== routes.AI) {
      return;
    }
    this.shouldRecordMetrics_ = false;

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

  private onHistorySearchRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.HISTORY_SEARCH_CLICK,
        'Settings.AiPage.HistorySearchEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().HISTORY_SEARCH);
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

  // SettingsViewMixin implementation.
  override getFocusConfig() {
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

    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    const ids = [
      'compare',
      'compose',
      'historySearch',
      'tabOrganization',
    ];
    assert(ids.includes(childViewId));

    let triggerId: string|null = null;
    switch (childViewId) {
      case 'compare':
        assert(this.showCompareControl_);
        triggerId = 'compareRowV2';
        break;
      case 'compose':
        assert(this.showComposeControl_);
        triggerId = 'composeRowV2';
        break;
      case 'historySearch':
        assert(this.showHistorySearchControl_);
        triggerId = 'historySearchRowV2';
        break;
      case 'tabOrganization':
        assert(this.showTabOrganizationControl_);
        triggerId = 'tabOrganizationRowV2';
        break;
    }

    assert(triggerId);

    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page': SettingsAiPageElement;
  }
}

customElements.define(SettingsAiPageElement.is, SettingsAiPageElement);
