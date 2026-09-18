// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {PrivacyGuideStep} from './constants.js';
import type {SettingsPrivacyGuidePageElement} from './privacy_guide_page.js';

export function getHtml(this: SettingsPrivacyGuidePageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="privacyGuideCard" @keydown="${this.onKeydown_}" part="privacyGuideCard"
    style="--privacy-guide-translate-multiplier: ${this.translateMultiplier_}">
  <div id="background" aria-hidden="true"
      ?hidden="${!this.showAnySettingFragment_()}"
      style="--privacy-guide-step: ${this.stepIndicatorModel_?.active ?? 0}">
    <picture id="backgroundClouds">
      <source
          srcset="./images/privacy_guide/clouds_graphic_dark.svg"
          media="(prefers-color-scheme: dark)">
      <img alt="" src="./images/privacy_guide/clouds_graphic.svg">
    </picture>
    <picture id="backgroundHills">
      <source
          srcset="./images/privacy_guide/hills_graphic_dark.svg"
          media="(prefers-color-scheme: dark)">
      <img alt="" src="./images/privacy_guide/hills_graphic.svg">
    </picture>
    <picture>
      <source
          srcset="./images/privacy_guide/horizon_graphic_dark.svg"
          media="(prefers-color-scheme: dark)">
      <img alt="" src="./images/privacy_guide/horizon_graphic.svg">
    </picture>
  </div>
  <cr-view-manager id="viewManager">
    <privacy-guide-welcome-fragment id="${PrivacyGuideStep.WELCOME}"
        class="managed-fragment" @start-button-click="${this.onStartButtonClick_}"
        slot="view">
    </privacy-guide-welcome-fragment>
    <privacy-guide-msbb-fragment id="${PrivacyGuideStep.MSBB}"
        class="managed-fragment" slot="view">
    </privacy-guide-msbb-fragment>
    <privacy-guide-history-sync-fragment
        id="${PrivacyGuideStep.HISTORY_SYNC}" class="managed-fragment"
        slot="view">
    </privacy-guide-history-sync-fragment>
    <privacy-guide-cookies-fragment id="${PrivacyGuideStep.COOKIES}"
        class="managed-fragment" slot="view">
    </privacy-guide-cookies-fragment>
    <privacy-guide-safe-browsing-fragment
        id="${PrivacyGuideStep.SAFE_BROWSING}" class="managed-fragment"
        slot="view">
    </privacy-guide-safe-browsing-fragment>
    <privacy-guide-completion-fragment
        id="${PrivacyGuideStep.COMPLETION}" class="managed-fragment"
        @back-button-click="${this.onBackButtonClick_}" slot="view">
    </privacy-guide-completion-fragment>
  </cr-view-manager>
  ${this.showAnySettingFragment_() ? html`
    <div id="settingFooter" class="footer hr">
      <cr-button id="backButton" role="button" @click="${this.onBackButtonClick_}"
          class="${this.computeBackButtonClass_()}">
        $i18n{privacyGuideBackButton}
      </cr-button>
      <step-indicator .model="${this.stepIndicatorModel_}"></step-indicator>
      <cr-button class="action-button" id="nextButton" role="button"
          tabindex="0" @click="${this.onNextButtonClick_}">
        $i18n{privacyGuideNextButton}
      </cr-button>
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
