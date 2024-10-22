// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageTabOrganizationInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {getTemplate} from './ai_tab_organization_subpage.html.js';
import {AiPageActions} from './constants.js';

export class SettingsAiTabOrganizationSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-ai-tab-organization-subpage';
  }

  static get template() {
    return getTemplate();
  }

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
