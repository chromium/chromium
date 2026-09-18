// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacyGuideCompletionFragmentElement} from './privacy_guide_completion_fragment.js';

export function getHtml(this: PrivacyGuideCompletionFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="welcome-completion-header">
  <picture>
    <source
        srcset="./images/privacy_guide/completion_banner_dark_v2.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/completion_banner_v2.svg">
  </picture>
  <h2 class="welcome-completion-header-label" tabindex="-1">
    $i18n{privacyGuideCompletionCardHeader}
  </h2>
  <div class="cr-secondary-text">${this.getSubheader_()}</div>
</div>
${this.shouldShowAiSettings_ ? html`
  <cr-link-row id="aiRow" using-slotted-label
      sub-label="$i18n{privacyGuideCompletionCardAiSettingsLabel}"
      start-icon="settings20:button-magic" external @click="${this.onAiRowClick_}">
    <div slot="label" class="label">$i18n{aiPageTitle}</div>
  </cr-link-row>
  <div aria-disabled="true" role="none">
    <a id="aiRowLink" href="ai" target="_blank" tabindex="-1"
        aria-disabled="true" role="none"></a>
  </div>
` : ''}
${this.shouldShowWaa_ ? html`
  <cr-link-row id="waaRow" using-slotted-label
      sub-label="$i18n{privacyGuideCompletionCardWaaSubLabel}"
      start-icon="settings:devices" external
      @click="${this.onWaaClick_}">
      <div slot="label" class="label">
        $i18n{privacyGuideCompletionCardWaaLabel}
      </div>
  </cr-link-row>
` : ''}
<div class="footer">
  <cr-button id="backButton" role="button" @click="${this.onBackButtonClick_}">
    $i18n{privacyGuideBackButton}
  </cr-button>
  <cr-button class="action-button" id="leaveButton"
      @click="${this.onLeaveButtonClick_}">
    $i18n{privacyGuideCompletionCardLeaveButton}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
