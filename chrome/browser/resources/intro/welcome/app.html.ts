// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WelcomeAppElement} from './app.js';

export function getHtml(this: WelcomeAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content">
  <img id="product-logo" src="images/product-logo.svg"
      alt="$i18n{productLogoAltText}">

  <h1 class="title">$i18n{welcomeTitle}</h1>

  ${this.showDefaultBrowserToggle_ ? html`
    <div id="default-browser-container">
      <span id="default-browser-label">$i18n{welcomeSetDefaultBrowser}</span>
      <cr-toggle id="default-browser-toggle"
          aria-labelledby="default-browser-label"
          ?checked="${!!this.setDefaultBrowser_}"
          ?disabled="${this.shouldDisableButtons_()}"
          @checked-changed="${this.onDefaultBrowserCheckedChanged_}">
      </cr-toggle>
    </div>`
  : ''}

  <cr-button id="acceptButton" class="action-button"
      ?disabled="${this.shouldDisableButtons_()}"
      @click="${this.onAcceptButtonClick_}">
    $i18n{welcomeStartButtonLabel}
  </cr-button>
</div>

${this.showMetricsOptIn_ ? html`
  <p id="footer" class="footer">
    <localized-link
        .linkDisabled="${this.shouldDisableButtons_()}"
        .localizedString="${this.getMetricsLabel_()}"
        @link-clicked="${this.onManageLinkClicked_}">
    </localized-link>
  </p>

  <cr-toast id="toast">
    <div id="toast-header">
      <div id="toast-title">$i18n{welcomeMetricsPopupTitle}</div>
      <cr-icon-button id="toastCloseButton" iron-icon="cr:close"
          aria-label="$i18n{welcomeMetricsPopupCloseButtonLabel}"
          @click="${this.onToastCloseButtonClick_}">
      </cr-icon-button>
    </div>
    <div id="toast-body">$i18n{welcomeMetricsPopupDescription}</div>
    <div id="toast-buttons">
      <cr-button id="toastActionButton" class="action-button"
          ?disabled="${this.shouldDisableButtons_()}"
          @click="${this.onToastActionButtonClick_}">
        ${this.getToastActionButtonLabel_()}
      </cr-button>
    </div>
  </cr-toast>`
: ''}
<!--_html_template_end_-->`;
  // clang-format on
}
