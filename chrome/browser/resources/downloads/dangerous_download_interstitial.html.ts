// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DownloadsDangerousDownloadInterstitialElement} from './dangerous_download_interstitial.js';

export function getHtml(this: DownloadsDangerousDownloadInterstitialElement) {
  return html`<!--_html_template_start_-->
<dialog id="dialog">
  <div id="interstitial-wrapper">
    <div id="main-content">
      <div id="icon-wrapper" role="img"
          aria-label="$i18n{accessibleLabelDangerous}">
        <cr-icon id="icon" icon="downloads:dangerous"></cr-icon>
      </div>
      <div id="main-message">
        <h1 id="title">$i18n{warningBypassInterstitialTitle}</h1>
        <p>$i18n{warningBypassPromptDescription}
          <!-- noopener cuts off the script connection between
            chrome://downloads, which has sensitive capabilities, and the newly
            opened web renderer, which may be more readily compromised. -->
          <a href="$i18n{blockedLearnMoreUrl}" target="_blank" rel="noopener"
              aria-label="$i18n{warningBypassPromptLearnMoreLinkAccessible}">
            $i18n{warningBypassPromptLearnMoreLink}
          </a>
        </p>
      </div>
      <div id="button-container">
        <cr-button class="secondary-button" id="continueAnywayButton"
            @click="${this.onContinueAnywayClick_}">
            $i18n{warningBypassInterstitialContinue}
        </cr-button>
        <cr-button id="backToSafetyButton"
            @click="${this.onBackToSafetyClick_}">
          $i18n{warningBypassInterstitialCancel}
        </cr-button>
      </div>
    </div>
    <div ?hidden="${this.hideSurveyAndDownloadButton_}"
      id="survey-and-download-button-wrapper">
      <div id="survey-wrapper">
        <p id="survey-title">$i18n{warningBypassInterstitialSurveyTitle}</p>
        <cr-radio-group
            aria-label="$i18n{warningBypassInterstitialSurveyTitleAccessible}"
            .selected="${this.selectedRadioOption_}"
            @selected-changed="${this.onSelectedRadioOptionChanged_}">
          <cr-radio-button name="CreatedFile"
              label="$i18n{warningBypassInterstitialSurveyCreatedFileAccessible}"
              hide-label-text>
            <span aria-hidden="true">
              $i18n{warningBypassInterstitialSurveyCreatedFile}
            </span>
          </cr-radio-button>
          <cr-radio-button name="TrustSite"
              label="${this.trustSiteLineAccessibleText}" hide-label-text>
            <span aria-hidden="true">${this.trustSiteLine}</span>
          </cr-radio-button>
          <cr-radio-button name="AcceptRisk"
              label="$i18n{warningBypassInterstitialSurveyAcceptRiskAccessible}"
              hide-label-text>
            <span aria-hidden="true">
              $i18n{warningBypassInterstitialSurveyAcceptRisk}
            </span>
          </cr-radio-button>
        </cr-radio-group>
      </div>
      <cr-button class="secondary-button" id="download-button"
          @click="${this.onDownloadClick_}">
        $i18n{warningBypassInterstitialDownload}
      </cr-button>
    </div>
  </div>
</dialog>
<!--_html_template_end_-->`;
}
