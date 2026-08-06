// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SpeedPageElement} from './speed_page.js';
import {NetworkPredictionOptions} from './constants.js';

export function getHtml(this: SpeedPageElement) {
  return html`<!--_html_template_start_-->
<settings-section
    ?show-send-feedback-button="${this.showSendFeedbackButton_()}"
    @send-feedback="${this.onSendFeedback_}"
    page-title="$i18n{speedPageTitle}">

  <settings-toggle-button id="preloadingToggle"
      pref-key="net.network_prediction_options"
      label="$i18n{preloadingPageTitle}"
      sub-label-with-link="$i18n{preloadingToggleSummary}"
      @sub-label-link-clicked="${this.onPreloadingSubLabelLinkClicked_}"
      .numericUncheckedValues="${this.numericUncheckedValues_}"
      .numericCheckedValue="${NetworkPredictionOptions.STANDARD}"
      @change="${this.onPreloadingStateChange_}">
  </settings-toggle-button>
  <cr-collapse ?opened="${this.isPreloadingEnabled_()}">
    <div class="cr-row continuation settings-section-bottom-padding">
      <settings-radio-group id="preloadingRadioGroup"
          pref-key="net.network_prediction_options"
          selectable-elements="settings-collapse-radio-button"
          @change="${this.onPreloadingStateChange_}">
        <settings-collapse-radio-button id="preloadingExtended"
            name="${NetworkPredictionOptions.EXTENDED}"
            pref-key="net.network_prediction_options"
            label="$i18n{preloadingPageExtendedPreloadingTitle}"
            sub-label="$i18n{preloadingPageExtendedPreloadingSummary}"
            expand-aria-label="
                $i18n{preloadingPageExtendedPreloadingExpandA11yLabel}"
            no-automatic-collapse>
          <div slot="collapse" class="settings-columned-section">
            <div class="column">
              <h2 class="description-header">
                $i18n{columnHeadingWhenOn}
              </h2>
              <ul>
                <li class="secondary">
                  $i18n{preloadingPageExtendedPreloadingWhenOnBulletOne}
                </li>
                <li class="secondary">
                  $i18n{preloadingPageExtendedPreloadingWhenOnBulletTwo}
                </li>
              </ul>
            </div>
            <div class="column">
              <h2 class="description-header">
                $i18n{columnHeadingConsider}
              </h2>
              <ul>
                <li class="secondary">
                  $i18n{preloadingPageThingsToConsiderBulletOne}
                </li>
                <li class="secondary">
                  $i18n{preloadingPageExtendedPreloadingThingsToConsiderBulletTwo}
                </li>
              </ul>
            </div>
          </div>
        </settings-collapse-radio-button>
        <settings-collapse-radio-button id="preloadingStandard"
            name="${NetworkPredictionOptions.STANDARD}"
            pref-key="net.network_prediction_options"
            label="$i18n{preloadingPageStandardPreloadingTitle}"
            sub-label="$i18n{preloadingPageStandardPreloadingSummary}"
            expand-aria-label="
                $i18n{preloadingPageStandardPreloadingExpandA11yLabel}"
            no-automatic-collapse>
          <div slot="collapse" class="settings-columned-section">
            <div class="column">
              <h2 class="description-header">
                $i18n{columnHeadingWhenOn}
              </h2>
              <ul>
                <li class="secondary">
                  $i18n{preloadingPageStandardPreloadingWhenOnBulletOne}
                </li>
                <li class="secondary">
                  $i18n{preloadingPageStandardPreloadingWhenOnBulletTwo}
                </li>
              </ul>
            </div>
            <div class="column">
              <h2 class="description-header">
                $i18n{columnHeadingConsider}
              </h2>
              <ul>
                <li class="secondary">
                  $i18n{preloadingPageThingsToConsiderBulletOne}
                </li>
              </ul>
            </div>
          </div>
        </settings-collapse-radio-button>
      </settings-radio-group>
    </div>
  </cr-collapse>

  ${this.cpuPerformanceEnabled_ ? html`
    <div class="hr"></div>
    <div class="cr-row">
      <div class="cpu-performance-override-header cr-padded-text">
        <div>$i18n{cpuPerformanceOverrideTitle}</div>
        <div class="cr-secondary-text">
          $i18n{cpuPerformanceOverrideDescription}
        </div>
      </div>
    </div>
    <div class="cr-row continuation">
      <div id="cpuPerformanceInfo" class="cr-padded-text">
        ${this.cpuPerformanceInfo_ ? html`
          <div class="cr-secondary-text">${this.cpuPerformanceModelLabel_}</div>
          <div class="cr-secondary-text">
            ${this.getCpuPerformanceNominalTierLabel_()}
          </div>
        ` : ''}
      </div>
      <settings-dropdown-menu id="cpuPerformanceOverrideDropdown"
          pref-key="cpu_performance_tier_override"
          .menuOptions="${this.cpuPerformanceTierOptions_}">
      </settings-dropdown-menu>
    </div>
  ` : ''}
</settings-section><!--_html_template_end_-->`;
}
