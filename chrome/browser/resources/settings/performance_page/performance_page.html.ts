// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PerformancePageElement} from './performance_page.js';

export function getHtml(this: PerformancePageElement) {
  return html`<!--_html_template_start_--><settings-section
    ?show-send-feedback-button="${this.showSendFeedbackButton_()}"
    @send-feedback="${this.onSendFeedback_}"
    page-title="$i18n{generalPageTitle}">
  <settings-toggle-button id="performanceInterventionToggleButton"
      @change="${this.onPerformanceInterventionToggleButtonChange_}"
      pref-key="performance_tuning.intervention_notification.enabled"
      label="$i18n{performanceInterventionEnabledLabel}"
      sub-label-with-link="$i18n{performanceInterventionEnabledDescription}"
      @sub-label-link-clicked="${this.onPerformanceInterventionSubLabelLinkClicked_}">
  </settings-toggle-button>
  <settings-toggle-button id="discardRingTreatmentToggleButton"
      @change="${this.onDiscardRingChange_}"
      pref-key="performance_tuning.discard_ring_treatment.enabled"
      label="$i18n{discardRingTreatmentEnabledLabel}"
      sub-label-with-link="
            $i18n{discardRingTreatmentEnabledDescriptionWithLearnLink}"
      @sub-label-link-clicked="${this.onDiscardRingTreatmentSubLabelLinkClicked_}">
  </settings-toggle-button>
  <cr-link-row
      label="$i18n{tabHoverPreviewCardLinkTitle}"
      sub-label="$i18n{tabHoverPreviewCardLinkSubtitle}"
      @click="${this.onTabHoverPreviewCardLinkClick_}" external>
  </cr-link-row>
  <tab-discard-exception-list id="exceptionList">
  </tab-discard-exception-list>
</settings-section>
<!--_html_template_end_-->`;
}
