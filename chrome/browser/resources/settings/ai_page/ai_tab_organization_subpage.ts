// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageTabOrganizationInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {getAiLearnMoreUrl} from './ai_learn_more_url_util.js';
import {getTemplate} from './ai_tab_organization_subpage.html.js';
import {AiEnterpriseFeaturePrefName, AiPageActions} from './constants.js';

const SettingsAiTabOrganizationSubpageElementBase = PrefsMixin(PolymerElement);

export class SettingsAiTabOrganizationSubpageElement extends
    SettingsAiTabOrganizationSubpageElementBase {
  static get is() {
    return 'settings-ai-tab-organization-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enterprisePref_: {
        type: Object,
        computed: `computePref(prefs.${
            AiEnterpriseFeaturePrefName.TAB_ORGANIZATION})`,
      },
    };
  }

  private enterprisePref_: chrome.settingsPrivate.PrefObject;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private recordInteractionMetrics_(
      interaction: AiPageTabOrganizationInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageTabOrganizationInteractions(
        interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  private onLearnMoreClick_() {
    this.recordInteractionMetrics_(
        AiPageTabOrganizationInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.TAB_ORGANIZATION_LEARN_MORE_CLICKED);
  }

  private getLearnMoreUrl_(): string {
    return getAiLearnMoreUrl(
        this.enterprisePref_,
        loadTimeData.getString('tabOrganizationLearnMoreUrl'),
        loadTimeData.getString('tabOrganizationLearnMoreManagedUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-tab-organization-subpage':
        SettingsAiTabOrganizationSubpageElement;
  }
}

customElements.define(
    SettingsAiTabOrganizationSubpageElement.is,
    SettingsAiTabOrganizationSubpageElement);
