// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WelcomeAppElement} from './app.js';

export function getHtml(this: WelcomeAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
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

<!--_html_template_end_-->`;
  // clang-format on
}
