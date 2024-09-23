// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';
import './tab_discard/exception_list.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './performance_page.html.js';
import type {ExceptionListElement} from './tab_discard/exception_list.js';

export const DISCARD_RING_PREF =
    'performance_tuning.discard_ring_treatment.enabled';

export const PERFORMANCE_INTERVENTION_NOTIFICATION_PREF =
    'performance_tuning.intervention_notification.enabled';

// browser_element_identifiers constants
const INACTIVE_TAB_SETTING_ELEMENT_ID = 'kInactiveTabSettingElementId';

const SettingsPerformancePageElementBase =
    HelpBubbleMixin(PrefsMixin(PolymerElement));

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

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isPerformanceInterventionUiEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isPerformanceInterventionUiEnabled');
        },
      },
    };
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private isPerformanceInterventionUiEnabled_: boolean;

  override ready() {
    super.ready();
    // Remove afterNextRender when feature is launched and dom-if is removed.
    afterNextRender(this, () => {
      const discardRingTreatmentToggleButton =
          this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#discardRingTreatmentToggleButton');
      if (discardRingTreatmentToggleButton) {
        this.registerHelpBubble(
            INACTIVE_TAB_SETTING_ELEMENT_ID,
            discardRingTreatmentToggleButton.getBubbleAnchor());
      }
    });
  }

  private onDiscardRingChange_() {
    this.metricsProxy_.recordDiscardRingTreatmentEnabledChanged(
        this.getPref<boolean>(DISCARD_RING_PREF).value);
  }

  private onDiscardRingTreatmentLearnMoreLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('discardRingTreatmentLearnMoreUrl'));
  }

  private onPerformanceInterventionLearnMoreLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('performanceInterventionLearnMoreUrl'));
  }

  private onTabHoverPreviewCardLinkClick_(): void {
    Router.getInstance().navigateTo(routes.APPEARANCE);
  }

  private onPerformanceInterventionToggleButtonChange_() {
    this.metricsProxy_.recordPerformanceInterventionToggleButtonChanged(
        this.getPref<boolean>(PERFORMANCE_INTERVENTION_NOTIFICATION_PREF)
            .value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
