// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import './tab_discard/exception_list.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {PerformanceBrowserProxyImpl, PerformanceFeedbackCategory} from './performance_browser_proxy.js';
import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getHtml} from './performance_page.html.js';
import type {ExceptionListElement} from './tab_discard/exception_list.js';

export const DISCARD_RING_PREF =
    'performance_tuning.discard_ring_treatment.enabled';

export const PERFORMANCE_INTERVENTION_NOTIFICATION_PREF =
    'performance_tuning.intervention_notification.enabled';

// browser_element_identifiers constants
const INACTIVE_TAB_SETTING_ELEMENT_ID = 'kInactiveTabSettingElementId';

const SettingsPerformancePageElementBase =
    HelpBubbleMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export interface SettingsPerformancePageElement {
  $: {
    exceptionList: ExceptionListElement,
  };
}

export class SettingsPerformancePageElement extends
    SettingsPerformancePageElementBase {
  static get is() {
    return 'settings-performance-page';
  }

  override render() {
    return getHtml.bind(this)();
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const discardRingTreatmentToggleButton =
        this.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#discardRingTreatmentToggleButton');
    if (discardRingTreatmentToggleButton) {
      this.registerHelpBubble(
          INACTIVE_TAB_SETTING_ELEMENT_ID,
          discardRingTreatmentToggleButton.getBubbleAnchor());
    }
  }

  protected onDiscardRingChange_() {
    this.metricsProxy_.recordDiscardRingTreatmentEnabledChanged(
        PrefService.getInstance().getPref<boolean>(DISCARD_RING_PREF).value);
  }

  protected onDiscardRingTreatmentSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('discardRingTreatmentLearnMoreUrl'));
  }

  protected onPerformanceInterventionSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('performanceInterventionLearnMoreUrl'));
  }

  protected onTabHoverPreviewCardLinkClick_(): void {
    Router.getInstance().navigateTo(routes.APPEARANCE);
  }

  protected onPerformanceInterventionToggleButtonChange_() {
    this.metricsProxy_.recordPerformanceInterventionToggleButtonChanged(
        PrefService.getInstance()
            .getPref<boolean>(PERFORMANCE_INTERVENTION_NOTIFICATION_PREF)
            .value);
  }

  // <if expr="_google_chrome">
  protected onSendFeedback_(e: Event) {
    e.stopPropagation();
    PerformanceBrowserProxyImpl.getInstance().openFeedbackDialog(
        PerformanceFeedbackCategory.NOTIFICATIONS);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

export {SettingsPerformancePageElement as PerformancePageElement};

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
